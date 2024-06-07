#pragma once
/*
 * backslash and Intlbackslash have same X11 keysym mapped in en-uk
 * keyboard layout.
 * So defining custom X11 keysym value for Intlbackslash key mapping.
 * @param[value] 0x100005c i.e: U+005C + 0x01000000(Reference: X11/keysymdef.h)
 */
#define XK_Intlbackslash 0x100005c
