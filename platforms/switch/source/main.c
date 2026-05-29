// Dusklight NX stub — phase 0 bring-up.
// Goal: prove the devkitPro toolchain produces a working .nro under the Dusklight name,
// before we start dragging Aurora/Dawn/Mesa-NVK into the build.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

static void print_header(void) {
    consoleClear();
    printf("\x1b[1;1H");
    printf("\x1b[1;33m");
    printf("=============================================\n");
    printf("  Dusklight NX  (stub bring-up, phase 0)\n");
    printf("=============================================\n");
    printf("\x1b[0m");
    printf("\n");
}

static void print_env(void) {
    AppletType at = appletGetAppletType();
    const char* atName = "?";
    switch (at) {
        case AppletType_None:                  atName = "None"; break;
        case AppletType_Default:               atName = "Default"; break;
        case AppletType_Application:           atName = "Application"; break;
        case AppletType_SystemApplet:          atName = "SystemApplet"; break;
        case AppletType_LibraryApplet:         atName = "LibraryApplet"; break;
        case AppletType_OverlayApplet:         atName = "OverlayApplet"; break;
        case AppletType_SystemApplication:     atName = "SystemApplication"; break;
        default:                               atName = "Unknown"; break;
    }
    printf("Applet type    : %s\n", atName);

    u64 lang = 0;
    if (R_SUCCEEDED(setGetSystemLanguage(&lang))) {
        printf("System language: 0x%016llx\n", (unsigned long long)lang);
    }

    SetSysFirmwareVersion fw = {0};
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) {
        printf("Firmware       : %u.%u.%u (%s)\n",
               fw.major, fw.minor, fw.micro, fw.display_version);
    }

    PsmChargerType ct = PsmChargerType_Unconnected;
    u32 batt = 0;
    if (R_SUCCEEDED(psmGetBatteryChargePercentage(&batt))) {
        psmGetChargerType(&ct);
        printf("Battery        : %u%% (%s)\n", batt,
               ct == PsmChargerType_Unconnected ? "unplugged" :
               ct == PsmChargerType_EnoughPower ? "charging (full)" :
               "charging");
    }

    printf("\n");
    printf("This .nro proves:\n");
    printf("  - devkitPro / devkitA64 toolchain works\n");
    printf("  - libnx services (set, setsys, psm, applet, hid) initialize\n");
    printf("  - NACP / icon / romfs section assembled by elf2nro\n");
    printf("  - Homebrew menu launches it under 'Dusklight NX'\n");
    printf("\n");
    printf("Next phases will replace this console UI with:\n");
    printf("  1. Aurora platform layer (libnx-native, no SDL3)\n");
    printf("  2. Dawn + Mesa NVK Vulkan backend\n");
    printf("  3. Game code (src/d, src/m_Do, ...) compiled for aarch64\n");
    printf("\n");
    printf("\x1b[1;32mPress (+) to exit.\x1b[0m\n");
}

int main(int argc, char* argv[]) {
    // Use Borealis-pattern explicit init order — see SWITCH_HOMEBREW_GUIDE.md.
    // Console mode is fine for this stub; once we add the real GPU backend, we
    // must drop consoleInit and switch to framebuffer or NVN/Vulkan presentation.
    consoleInit(NULL);

    // Optional services we want available even in the stub, so we can verify
    // they initialize cleanly under the homebrew sandbox.
    setInitialize();
    setsysInitialize();
    psmInitialize();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    print_header();
    print_env();
    consoleUpdate(NULL);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(NULL);
    }

    psmExit();
    setsysExit();
    setExit();
    consoleExit(NULL);
    return 0;
}
