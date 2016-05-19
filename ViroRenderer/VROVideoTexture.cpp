//
//  VROVideoTexture.cpp
//  ViroRenderer
//
//  Created by Raj Advani on 1/8/16.
//  Copyright © 2016 Viro Media. All rights reserved.
//

#include "VROVideoTexture.h"
#include "VRORenderContext.h"
#include "VROFrameSynchronizer.h"
#include "VRODriverMetal.h"
#include "VROLog.h"
#include "VROTextureSubstrateMetal.h"
#include "VROTime.h"
#include "VROAllocationTracker.h"
#include "VROVideoTextureCache.h"
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

# define ONE_FRAME_DURATION 0.03

VROVideoTexture::VROVideoTexture() :
    _notificationToken(nullptr),
    _mediaReady(false),
    _paused(true),
    _currentTextureIndex(0),
    _videoTextureCache(nullptr) {
    
    _player = [[AVPlayer alloc] init];
    
    NSDictionary *pixBuffAttributes = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)};
    _videoOutput = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:pixBuffAttributes];
        
    ALLOCATION_TRACKER_ADD(VideoTextures, 1);
}

VROVideoTexture::~VROVideoTexture() {
    delete (_videoTextureCache);
    ALLOCATION_TRACKER_SUB(VideoTextures, 1);
}

#pragma mark - Recorded Video Playback

void VROVideoTexture::prewarm() {
    [_player play];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [_player pause];
    });
}

void VROVideoTexture::play() {
    _paused = false;
    [_player play];
}

void VROVideoTexture::pause() {
    [_player pause];
    _paused = true;
}

bool VROVideoTexture::isPaused() {
    return _paused;
}

void VROVideoTexture::loadVideo(NSURL *url,
                                std::shared_ptr<VROFrameSynchronizer> frameSynchronizer,
                                const VRODriver &driver) {
    
    _mediaReady = false;
    
    _videoTextureCache = driver.newVideoTextureCache();
    _videoQueue = dispatch_queue_create("video_output_queue", DISPATCH_QUEUE_SERIAL);
    _videoPlaybackDelegate = [[VROVideoPlaybackDelegate alloc] initWithVROVideoTexture:this];
    [_videoOutput setDelegate:_videoPlaybackDelegate queue:_videoQueue];
    
    frameSynchronizer->addFrameListener(shared_from_this());
    
    /*
     Sets up player item and adds video output to it. The tracks property of an asset is
     loaded via asynchronous key value loading, to access the preferred transform of a
     video track used to orientate the video while rendering.
     
     After adding the video output, we request a notification of media change in order
     to restart the CADisplayLink.
     */
    [[_player currentItem] removeOutput:_videoOutput];
    
    AVPlayerItem *item = [AVPlayerItem playerItemWithURL:url];
    AVAsset *asset = [item asset];
    
    [asset loadValuesAsynchronouslyForKeys:@[@"tracks"] completionHandler:^{
        
        if ([asset statusOfValueForKey:@"tracks" error:nil] == AVKeyValueStatusLoaded) {
            NSArray *tracks = [asset tracksWithMediaType:AVMediaTypeVideo];
            if ([tracks count] > 0) {
                
                // Choose the first video track.
                AVAssetTrack *videoTrack = [tracks objectAtIndex:0];
                [videoTrack loadValuesAsynchronouslyForKeys:@[@"preferredTransform"] completionHandler:^{
                    
                    if ([videoTrack statusOfValueForKey:@"preferredTransform" error:nil] == AVKeyValueStatusLoaded) {
                        CGAffineTransform preferredTransform = [videoTrack preferredTransform];
                        
                        /*
                         The orientation of the camera while recording affects the orientation
                         of the images received from an AVPlayerItemVideoOutput. Here we compute a
                         rotation that is used to correctly orientate the video.
                         */
                        _preferredRotation = -1 * atan2(preferredTransform.b, preferredTransform.a);
                        addLoopNotification(item);
                        
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [item addOutput:_videoOutput];
                            [_player replaceCurrentItemWithPlayerItem:item];
                            [_videoOutput requestNotificationOfMediaDataChangeWithAdvanceInterval:ONE_FRAME_DURATION];
                        });
                    }
                }];
            }
        }
    }];
}

void VROVideoTexture::addLoopNotification(AVPlayerItem *item) {
    if (_notificationToken) {
        _notificationToken = nil;
    }
    
    /*
     Setting actionAtItemEnd to None prevents the movie from getting paused at item end. A very simplistic, and not gapless, looped playback.
     */
    _player.actionAtItemEnd = AVPlayerActionAtItemEndNone;
    _notificationToken = [[NSNotificationCenter defaultCenter] addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
                                                                           object:item
                                                                            queue:[NSOperationQueue mainQueue]
                                                                       usingBlock:^(NSNotification *note) {
                          [[_player currentItem] seekToTime:kCMTimeZero];
                          }
                          ];
}

void VROVideoTexture::onFrameWillRender(const VRORenderContext &context) {
    /*
     Stuttering is significantly reduced by placing this code in willRender() as opposed
     to didRender(). Reason unknown: contention of resources somewhere?
     */
    if (!_mediaReady) {
        return;
    }
    
    _currentTextureIndex = (_currentTextureIndex + 1) % kInFlightVideoTextures;
    
    double timestamp = CACurrentMediaTime();
    double duration = .01667;
    
    /*
     The callback gets called once every frame. Compute the next time the screen will be
     refreshed, and copy the pixel buffer for that time. This pixel buffer can then be processed
     and later rendered on screen.
     */
    CFTimeInterval nextVSync = timestamp + duration;
    CMTime outputItemTime = [_videoOutput itemTimeForHostTime:nextVSync];
    
    if ([_videoOutput hasNewPixelBufferForItemTime:outputItemTime]) {
        CVPixelBufferRef pixelBuffer = [_videoOutput copyPixelBufferForItemTime:outputItemTime
                                                             itemTimeForDisplay:NULL];
        
        if (pixelBuffer != nullptr) {
            displayPixelBuffer(pixelBuffer);
            CFRelease(pixelBuffer);
        }
    }
}

void VROVideoTexture::onFrameDidRender(const VRORenderContext &context) {
   
}

void VROVideoTexture::displayPixelBuffer(CVPixelBufferRef pixelBuffer) {
    setSubstrate(VROTextureType::Quad, std::move(_videoTextureCache->createTextureSubstrate(pixelBuffer)));
}

@interface VROVideoPlaybackDelegate ()

@property (readonly) VROVideoTexture *texture;

@end

@implementation VROVideoPlaybackDelegate

- (id)initWithVROVideoTexture:(VROVideoTexture *)texture {
    self = [super init];
    if (self) {
        _texture = texture;
    }
    
    return self;
}

- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender {
    self.texture->setMediaReady(true);
}

@end

#pragma mark - Live Video Playback

void VROVideoTexture::displayCamera(AVCaptureDevicePosition position,
                                    std::shared_ptr<VROFrameSynchronizer> frameSynchronizer,
                                    const VRODriver &driver) {
    
    frameSynchronizer->addFrameListener(shared_from_this());
    _videoDelegate = [[VROVideoCaptureDelegate alloc] initWithVROVideoTexture:this];
    _videoTextureCache = driver.newVideoTextureCache();
    
    // Create a capture session
    _captureSession = [[AVCaptureSession alloc] init];
    if (!_captureSession) {
        pinfo("ERROR: Couldnt create a capture session");
        pabort();
    }
    
    [_captureSession beginConfiguration];
    [_captureSession setSessionPreset:AVCaptureSessionPresetLow];
    
    // Get the a video device with preference to the front facing camera
    AVCaptureDevice *videoDevice = nil;
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    for (AVCaptureDevice *device in devices) {
        if ([device position] == position) {
            videoDevice = device;
        }
    }
    
    if (videoDevice == nil) {
        videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }
    
    if (videoDevice == nil) {
        pinfo("ERROR: Couldnt create a AVCaptureDevice");
        pabort();
    }
    
    NSError *error;
    
    // Device input
    AVCaptureDeviceInput *deviceInput = [AVCaptureDeviceInput deviceInputWithDevice:videoDevice error:&error];
    if (error) {
        pinfo("ERROR: Couldnt create AVCaptureDeviceInput");
        pabort();
    }
    
    [_captureSession addInput:deviceInput];
    
    // Create the output for the capture session.
    AVCaptureVideoDataOutput *dataOutput = [[AVCaptureVideoDataOutput alloc] init];
    [dataOutput setAlwaysDiscardsLateVideoFrames:YES];
    
    // Set the color space.
    [dataOutput setVideoSettings:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_32BGRA]
                                                             forKey:(id)kCVPixelBufferPixelFormatTypeKey]];
    
    // Set dispatch to be on the main thread to create the texture in memory and allow Metal to use it for rendering
    [dataOutput setSampleBufferDelegate:_videoDelegate queue:dispatch_get_main_queue()];
    
    [_captureSession addOutput:dataOutput];
    [_captureSession commitConfiguration];
    [_captureSession startRunning];
    
    _mediaReady = true;
}

@interface VROVideoCaptureDelegate ()

@property (readonly) VROVideoTexture *texture;

@end

@implementation VROVideoCaptureDelegate {
    
    id <MTLTexture> _videoTexture[kInFlightVideoTextures];
    
}

- (id)initWithVROVideoTexture:(VROVideoTexture *)texture {
    self = [super init];
    if (self) {
        _texture = texture;
    }
    
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
fromConnection:(AVCaptureConnection *)connection {
    
    VROVideoTextureCache *videoTextureCache = _texture->getVideoTextureCache();
    _texture->setSubstrate(VROTextureType::Quad, videoTextureCache->createTextureSubstrate(sampleBuffer));    
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
fromConnection:(AVCaptureConnection *)connection {
    
}

@end