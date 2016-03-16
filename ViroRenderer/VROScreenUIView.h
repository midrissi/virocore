//
//  VROScreenUIView.h
//  ViroRenderer
//
//  Created by Raj Advani on 1/12/16.
//  Copyright © 2016 Viro Media. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "VRORenderContext.h"
#import "VROLayer.h"
#import "VROReticle.h"
#import <memory>

class VROEye;

/*
 UIView for rendering a HUD in screen space.
 */
@interface VROScreenUIView : UIView

- (instancetype)init;

- (void)updateWithContext:(const VRORenderContext *)context;
- (void)renderEye:(VROEye *)eye withContext:(const VRORenderContext *)context;

- (void)setReticleEnabled:(BOOL)enabled;
- (void)setNeedsUpdate;

@property (readonly, nonatomic) VROReticle *reticle;

@end
