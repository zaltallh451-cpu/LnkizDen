#ifndef ONIONHEN_COMPANION_WEB_DIR
#define ONIONHEN_COMPANION_WEB_DIR "../../web/companion"
#endif

__asm__(
    ".section .rodata\n"
    ".global companion_index_start\n"
    ".align 4\n"
    "companion_index_start:\n"
    ".incbin \"" ONIONHEN_COMPANION_WEB_DIR "/index.html\"\n"
    ".global companion_index_end\n"
    "companion_index_end:\n"
    ".global companion_css_start\n"
    ".align 4\n"
    "companion_css_start:\n"
    ".incbin \"" ONIONHEN_COMPANION_WEB_DIR "/companion.css\"\n"
    ".global companion_css_end\n"
    "companion_css_end:\n"
    ".global companion_js_start\n"
    ".align 4\n"
    "companion_js_start:\n"
    ".incbin \"" ONIONHEN_COMPANION_WEB_DIR "/companion.js\"\n"
    ".global companion_js_end\n"
    "companion_js_end:\n");
