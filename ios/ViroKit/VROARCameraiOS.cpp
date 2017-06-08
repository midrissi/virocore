//
//  VROARCameraiOS.cpp
//  ViroKit
//
//  Created by Raj Advani on 6/6/17.
//  Copyright © 2017 Viro Media. All rights reserved.
//

#include "Availability.h"
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#include "VROARCameraiOS.h"
#include "VROConvert.h"
#include "VROViewport.h"
#include "VROLog.h"
#include "VROCameraTexture.h"
#include "VROVector3f.h"

VROARCameraiOS::VROARCameraiOS(ARCamera *camera, VROCameraOrientation orientation) :
    _camera(camera),
    _orientation(orientation) {
    
}

VROARCameraiOS::~VROARCameraiOS() {
    
}

VROARTrackingState VROARCameraiOS::getTrackingState() const {
    switch (_camera.trackingState) {
        case ARTrackingStateNotAvailable:
            return VROARTrackingState::Unavailable;
        case ARTrackingStateLimited:
            return VROARTrackingState::Limited;
        case ARTrackingStateNormal:
            return VROARTrackingState::Normal;
        default:
            return VROARTrackingState::Unavailable;
    };
}

VROARTrackingStateReason VROARCameraiOS::getLimitedTrackingStateReason() const {
    switch (_camera.trackingStateReason) {
        case ARTrackingStateReasonNone:
            return VROARTrackingStateReason::None;
        case ARTrackingStateReasonExcessiveMotion:
            return VROARTrackingStateReason::ExcessiveMotion;
        case ARTrackingStateReasonInsufficientFeatures:
            return VROARTrackingStateReason::InsufficientFeatures;
        default:
            return VROARTrackingStateReason::None;
    }
}

VROMatrix4f VROARCameraiOS::getRotation() const {
    VROMatrix4f matrix = VROConvert::toMatrix4f(_camera.transform);
    // Remove the translation; this is returned via getPosition()
    matrix[12] = 0;
    matrix[13] = 0;
    matrix[14] = 0;
    
    return matrix;
}

VROVector3f VROARCameraiOS::getPosition() const {
    VROMatrix4f matrix = VROConvert::toMatrix4f(_camera.transform);
    return { matrix[12], matrix[13], matrix[14] };
}

VROMatrix4f VROARCameraiOS::getProjection(VROViewport viewport, float near, float far, VROFieldOfView *outFOV) const {
    // TODO Output the FOV!
    return VROConvert::toMatrix4f([_camera projectionMatrixWithViewportSize:CGSizeMake(viewport.getWidth() / viewport.getContentScaleFactor(),
                                                                                       viewport.getHeight() / viewport.getContentScaleFactor())
                                                                orientation:VROConvert::toDeviceOrientation(_orientation)
                                                                      zNear:near
                                                                       zFar:far]);
}

VROVector3f VROARCameraiOS::getImageSize() const {
    CGSize size = _camera.imageResolution;
    return { (float) size.width, (float) size.height, 0 };
}

#endif
