//
//  Surface_JNI.cpp
//  ViroRenderer
//
//  Copyright © 2016 Viro Media. All rights reserved.
//

#include <jni.h>
#include <memory>
#include <VROVideoSurface.h>
#include <VROVideoTextureAVP.h>
#include "VRONode.h"

#include "VROMaterial.h"
#include "PersistentRef.h"
#include "VROFrameSynchronizer.h"
#include "VRORenderer_JNI.h"
#include "Node_JNI.h"
#include "RenderContext_JNI.h"
#include "VideoTexture_JNI.h"
#include "Material_JNI.h"

#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_viro_renderer_jni_SurfaceJni_##method_name

namespace Surface {
    inline jlong jptr(std::shared_ptr<VROSurface> surface) {
        PersistentRef<VROSurface> *persistedSurface = new PersistentRef<VROSurface>(surface);
        return reinterpret_cast<intptr_t>(persistedSurface);
    }

    inline std::shared_ptr<VROSurface> native(jlong ptr) {
        PersistentRef<VROSurface> *persistedSurface = reinterpret_cast<PersistentRef<VROSurface> *>(ptr);
        return persistedSurface->get();
    }
}

extern "C" {

JNI_METHOD(jlong, nativeCreateSurface)(JNIEnv *env,
                                        jobject object,
                                        jfloat width,
                                        jfloat height) {
    std::shared_ptr<VROSurface> surface = VROSurface::createSurface(width, height);
    return Surface::jptr(surface);
}

JNI_METHOD(jlong, nativeCreateSurfaceFromSurface)(JNIEnv *env,
                                       jobject object,
                                       jfloat width,
                                       jfloat height,
                                       jlong oldSurface) {
    std::shared_ptr<VROSurface> surface = VROSurface::createSurface(width, height);
    std::vector<std::shared_ptr<VROMaterial>> materials = Surface::native(oldSurface)->getMaterials();
    if (materials.size() > 0) {
        surface->getMaterials().push_back(materials[0]);
    }
    return Surface::jptr(surface);
}

JNI_METHOD(void, nativeDestroySurface)(JNIEnv *env,
                                        jclass clazz,
                                        jlong nativeSurface) {
    VROPlatformDispatchAsyncRenderer([nativeSurface] {
        delete reinterpret_cast<PersistentRef<VROSurface> *>(nativeSurface);
    });
}

JNI_METHOD(void, nativeAttachToNode)(JNIEnv *env,
                                     jclass clazz,
                                     jlong surfaceRef,
                                     jlong nodeRef) {
    VROPlatformDispatchAsyncRenderer([surfaceRef, nodeRef] {
        std::shared_ptr<VROSurface> surface = Surface::native(surfaceRef);
        std::shared_ptr<VRONode> node = Node::native(nodeRef);
        node->setGeometry(surface);
    });
}

JNI_METHOD(void, nativeSetVideoTexture)(JNIEnv *env,
                                        jobject obj,
                                        jlong surfaceRef,
                                        jlong textureRef) {
    VROPlatformDispatchAsyncRenderer([surfaceRef, textureRef] {
        std::shared_ptr<VROSurface> surface = Surface::native(surfaceRef);
        passert (!surface->getMaterials().empty());
        std::shared_ptr<VROVideoTexture> videoTexture = VideoTexture::native(textureRef);

        std::shared_ptr<VROMaterial> &material = surface->getMaterials().front();
        material->getDiffuse().setTexture(videoTexture);
    });
}

JNI_METHOD(void, nativeSetImageTexture)(JNIEnv *env,
                                        jobject obj,
                                        jlong surfaceRef,
                                        jlong textureRef) {
    std::shared_ptr<VROTexture> imageTexture = Texture::native(textureRef);
    VROPlatformDispatchAsyncRenderer([surfaceRef, imageTexture] {
        std::shared_ptr<VROSurface> surface = Surface::native(surfaceRef);
        passert (!surface->getMaterials().empty());

        std::shared_ptr<VROMaterial> &material = surface->getMaterials().front();
        material->getDiffuse().setTexture(imageTexture);
    });
}

JNI_METHOD(void, nativeSetMaterial)(JNIEnv *env,
                                    jobject obj,
                                    jlong surfaceRef,
                                    jlong materialRef) {
    std::shared_ptr<VROMaterial> mat = Material::native(materialRef);
    VROPlatformDispatchAsyncRenderer([surfaceRef, mat] {
        std::shared_ptr<VROSurface> surface = Surface::native(surfaceRef);
        // Copy the defined material into the surface material
        surface->getMaterials().front()->copyFrom(mat);
    });
}

JNI_METHOD(void, nativeClearMaterial)(JNIEnv *env,
                                      jobject obj,
                                      jlong surfaceRef) {
    VROPlatformDispatchAsyncRenderer([surfaceRef] {
        std::shared_ptr<VROSurface> surface = Surface::native(surfaceRef);
        surface->getMaterials().clear();
    });
}

}  // extern "C"