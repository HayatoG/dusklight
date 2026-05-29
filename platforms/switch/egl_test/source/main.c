// Minimal switch-mesa EGL probe. Goal: determine whether public devkitPro
// switch-mesa libEGL.a + libGLESv2.a actually work under Eden emulator.
// If this prints "EGL ok" to sdmc:/egl_test.log + clears the screen blue,
// switch-mesa is functional in Eden and our Dawn integration is the
// blocker. If this hangs or fails, Eden lacks nv:* services (Gap F4).
//
// Pattern: switchbrew/switch-examples/graphics/opengl/simple_triangle

#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>  // GLES 3.1 — SSBO/UBO cap constants (vertex-pulling gate, G0)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Full Application heap so libnx doesn't restrict us
u32    __nx_applet_type = AppletType_Application;
size_t __nx_heap_size   = 0;

static FILE* g_log = NULL;

static void log_init(void) {
    g_log = fopen("sdmc:/egl_test.log", "w");
    if (g_log) {
        fputs("[EGL-TEST] init\n", g_log);
        fflush(g_log);
    }
}

static void log_line(const char* s) {
    if (g_log) {
        fputs(s, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
    // Also echo to debug stream
    svcOutputDebugString(s, strlen(s));
    char nl = '\n';
    svcOutputDebugString(&nl, 1);
}

static void log_int(const char* label, int v) {
    char b[128];
    snprintf(b, sizeof(b), "%s = %d (0x%x)", label, v, v);
    log_line(b);
}

static void log_ptr(const char* label, const void* p) {
    char b[128];
    snprintf(b, sizeof(b), "%s = %p", label, p);
    log_line(b);
}

static void log_cap(const char* label, GLenum e) {
    GLint v = -1;
    glGetIntegerv(e, &v);
    char b[160];
    snprintf(b, sizeof(b), "%s = %d", label, (int)v);
    log_line(b);
}

int main(int argc, char* argv[]) {
    log_init();
    log_line("[EGL-TEST] main entered");

    NWindow* nw = nwindowGetDefault();
    log_ptr("[EGL-TEST] nwindowGetDefault", nw);
    if (!nw) {
        log_line("[EGL-TEST] FATAL: no NWindow");
        return 1;
    }

    nwindowSetDimensions(nw, 1280, 720);
    log_line("[EGL-TEST] nwindowSetDimensions OK");

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    log_ptr("[EGL-TEST] eglGetDisplay", (void*)display);
    if (display == EGL_NO_DISPLAY) {
        log_int("[EGL-TEST] FATAL: eglGetError", eglGetError());
        return 2;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        log_int("[EGL-TEST] FATAL: eglInitialize, eglGetError", eglGetError());
        return 3;
    }
    log_int("[EGL-TEST] EGL major", major);
    log_int("[EGL-TEST] EGL minor", minor);
    log_line(eglQueryString(display, EGL_VENDOR));
    log_line(eglQueryString(display, EGL_VERSION));
    log_line(eglQueryString(display, EGL_EXTENSIONS));

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        log_int("[EGL-TEST] FATAL: eglBindAPI, eglGetError", eglGetError());
        return 4;
    }
    log_line("[EGL-TEST] eglBindAPI(EGL_OPENGL_ES_API) OK");

    EGLConfig config;
    EGLint num_config = 0;
    static const EGLint cfg_attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_NONE
    };
    if (!eglChooseConfig(display, cfg_attrs, &config, 1, &num_config) || num_config == 0) {
        log_int("[EGL-TEST] FATAL: eglChooseConfig, eglGetError", eglGetError());
        return 5;
    }
    log_int("[EGL-TEST] eglChooseConfig num_config", num_config);

    EGLSurface surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)nw, NULL);
    log_ptr("[EGL-TEST] eglCreateWindowSurface", (void*)surface);
    if (surface == EGL_NO_SURFACE) {
        log_int("[EGL-TEST] FATAL: eglGetError", eglGetError());
        return 6;
    }

    static const EGLint ctx_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attrs);
    log_ptr("[EGL-TEST] eglCreateContext", (void*)context);
    if (context == EGL_NO_CONTEXT) {
        log_int("[EGL-TEST] FATAL: eglGetError", eglGetError());
        return 7;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        log_int("[EGL-TEST] FATAL: eglMakeCurrent, eglGetError", eglGetError());
        return 8;
    }
    log_line("[EGL-TEST] eglMakeCurrent OK");
    log_line((const char*)glGetString(GL_VENDOR));
    log_line((const char*)glGetString(GL_RENDERER));
    log_line((const char*)glGetString(GL_VERSION));
    log_line((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // ===== G0: GL caps that gate the native-GLES aurora backend (PLAN_GLES.md) =====
    // The vertex-pulling architecture (GX verts in SSBOs, read in the VERTEX shader by
    // gl_VertexID) REQUIRES >= 2 vertex-stage SSBOs. If MAX_VERTEX_SHADER_STORAGE_BLOCKS
    // is 0, we must fall back (samplerBuffer / CPU vertex pre-expansion). HIGHEST risk.
    log_line("[EGL-TEST] ===== GL CAPS (vertex-pulling gate) =====");
    log_cap("GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS",   GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS);
    log_cap("GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS", GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS);
    log_cap("GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS",  GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS);
    log_cap("GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS", GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    log_cap("GL_MAX_SHADER_STORAGE_BLOCK_SIZE",      GL_MAX_SHADER_STORAGE_BLOCK_SIZE);
    log_cap("GL_MAX_UNIFORM_BLOCK_SIZE",             GL_MAX_UNIFORM_BLOCK_SIZE);
    log_cap("GL_MAX_UNIFORM_BUFFER_BINDINGS",        GL_MAX_UNIFORM_BUFFER_BINDINGS);
    log_cap("GL_MAX_VERTEX_UNIFORM_BLOCKS",          GL_MAX_VERTEX_UNIFORM_BLOCKS);
    log_cap("GL_MAX_TEXTURE_IMAGE_UNITS",            GL_MAX_TEXTURE_IMAGE_UNITS);
    log_cap("GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS",     GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    log_cap("GL_MAX_VERTEX_ATTRIBS",                 GL_MAX_VERTEX_ATTRIBS);
    log_cap("GL_MAX_TEXTURE_SIZE",                   GL_MAX_TEXTURE_SIZE);
    log_line("[EGL-TEST] ===== GL_EXTENSIONS =====");
    log_line((const char*)glGetString(GL_EXTENSIONS));
    log_line("[EGL-TEST] ===== end caps =====");

    // Pad init so we can exit cleanly via + button
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    log_line("[EGL-TEST] entering render loop");
    int frame = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;

        // Cycle clear color so we can see frames actually present
        float t = (float)(frame % 120) / 120.0f;
        glClearColor(t, 0.2f, 1.0f - t, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(display, surface);
        frame++;

        if (frame == 1 || frame == 60) {
            char b[64];
            snprintf(b, sizeof(b), "[EGL-TEST] frame %d rendered+swapped", frame);
            log_line(b);
        }
        if (frame >= 600) {  // ~10s @ 60fps, then auto-exit
            log_line("[EGL-TEST] frame limit reached, exiting");
            break;
        }
    }

    log_line("[EGL-TEST] shutting down");
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    if (g_log) { fclose(g_log); g_log = NULL; }
    return 0;
}
