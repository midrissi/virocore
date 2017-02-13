//
// AmbientLight_JNI.cpp
// ViroRenderer
//
// Copyright © 2016 Viro Media. All rights reserved.

#include <jni.h>
#include <VROLight.h>
#include <PersistentRef.h>
#include <VRONode.h>
#include <VROPlatformUtil.h>
#include "Node_JNI.h"

#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_viro_renderer_jni_AmbientLightJni_##method_name

namespace AmbientLight {
    inline jlong jptr(std::shared_ptr<VROLight> shared_node) {
        PersistentRef<VROLight> *native_light = new PersistentRef<VROLight>(shared_node);
        return reinterpret_cast<intptr_t>(native_light);
    }

    inline std::shared_ptr<VROLight> native(jlong ptr) {
        PersistentRef<VROLight> *persistentBox = reinterpret_cast<PersistentRef<VROLight> *>(ptr);
        return persistentBox->get();
    }
}

extern "C" {

JNI_METHOD(jlong, nativeCreateAmbientLight)(JNIEnv *env,
                                                jclass clazz,
                                                jlong color) {
    std::shared_ptr<VROLight> ambientLight = std::make_shared<VROLight>(VROLightType::Ambient);

    // Get the color
    float r = ((color >> 16) & 0xFF) / 255.0;
    float g = ((color >> 8) & 0xFF) / 255.0;
    float b = (color & 0xFF) / 255.0;

    VROVector3f vecColor(r, g, b);
    ambientLight->setColor(vecColor);
    return AmbientLight::jptr(ambientLight);
}

JNI_METHOD(void, nativeDestroyAmbientLight)(JNIEnv *env,
                                   jclass clazz,
                                   jlong nativeLightRef) {
    VROPlatformDispatchAsyncRenderer([nativeLightRef] {
        delete reinterpret_cast<PersistentRef<VROLight> *>(nativeLightRef);
    });
}

JNI_METHOD(void, nativeAddToNode)(JNIEnv *env,
                                     jclass clazz,
                                     jlong native_light_ref,
                                     jlong native_node_ref) {
    VROPlatformDispatchAsyncRenderer([native_light_ref, native_node_ref] {
        std::shared_ptr<VROLight> light = AmbientLight::native(native_light_ref);
        Node::native(native_node_ref)->addLight(light);
    });
}

JNI_METHOD(void, nativeRemoveFromNode)(JNIEnv *env,
                                       jclass clazz,
                                       jlong native_light_ref,
                                       jlong native_node_ref) {
    VROPlatformDispatchAsyncRenderer([native_light_ref, native_node_ref] {
        std::shared_ptr<VROLight> light = AmbientLight::native(native_light_ref);
        Node::native(native_node_ref)->removeLight(light);
    });
}

JNI_METHOD(void, nativeSetColor)(JNIEnv *env,
                                       jclass clazz,
                                       jlong native_light_ref,
                                       jlong color) {
    VROPlatformDispatchAsyncRenderer([native_light_ref, color] {
        std::shared_ptr<VROLight> light = AmbientLight::native(native_light_ref);

        // Get the color
        float r = ((color >> 16) & 0xFF) / 255.0;
        float g = ((color >> 8) & 0xFF) / 255.0;
        float b = (color & 0xFF) / 255.0;

        VROVector3f vecColor(r, g, b);

        light->setColor(vecColor);
    });
}

} // extern "C"