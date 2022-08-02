/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef __QNX__
#include <libgen.h>
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#if defined(DINGUX)
#include "dingux/dingux_utils.h"
#endif

#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <file/file_path.h>
#include <string/stdstring.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configuration.h"
#include "file_path_special.h"

#include "msg_hash.h"
#include "paths.h"
#include "verbosity.h"

bool fill_pathname_application_data(char *s, size_t len)
{
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef LEGACY_WIN32
   const char *appdata = getenv("APPDATA");

   if (appdata)
   {
      strlcpy(s, appdata, len);
      return true;
   }
#else
   const wchar_t *appdataW = _wgetenv(L"APPDATA");

   if (appdataW)
   {
      char *appdata = utf16_to_utf8_string_alloc(appdataW);

      if (appdata)
      {
         strlcpy(s, appdata, len);
         free(appdata);
         return true;
      }
   }
#endif

#elif defined(OSX)
   const char *appdata = getenv("HOME");

   if (appdata)
   {
      fill_pathname_join(s, appdata,
            "Library/Application Support/RetroArch", len);
      return true;
   }
#elif defined(RARCH_UNIX_CWD_ENV)
   getcwd(s, len);
   return true;
#elif defined(DINGUX)
   dingux_get_base_path(s, len);
   return true;
#elif !defined(RARCH_CONSOLE)
   const char *xdg     = getenv("XDG_CONFIG_HOME");
   const char *appdata = getenv("HOME");

   /* XDG_CONFIG_HOME falls back to $HOME/.config with most Unix systems */
   /* On Haiku, it is set by default to /home/user/config/settings */
   if (xdg)
   {
      fill_pathname_join(s, xdg, "retroarch/", len);
      return true;
   }

   if (appdata)
   {
#ifdef __HAIKU__
      /* in theory never used as Haiku has XDG_CONFIG_HOME set by default */
      fill_pathname_join(s, appdata,
            "config/settings/retroarch/", len);
#else
      fill_pathname_join(s, appdata,
            ".config/retroarch/", len);
#endif
      return true;
   }
#endif

   return false;
}

#ifdef HAVE_XMB
const char* xmb_theme_ident(void);
#endif

void fill_pathname_application_special(char *s,
      size_t len, enum application_special_type type)
{
   switch (type)
   {
      case APPLICATION_SPECIAL_DIRECTORY_CONFIG:
         {
            settings_t *settings        = config_get_ptr();
            const char *dir_menu_config = settings->paths.directory_menu_config;

            /* Try config directory setting first,
             * fallback to the location of the current configuration file. */
            if (!string_is_empty(dir_menu_config))
               strlcpy(s, dir_menu_config, len);
            else if (!path_is_empty(RARCH_PATH_CONFIG))
               fill_pathname_basedir(s, path_get(RARCH_PATH_CONFIG), len);
         }
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS:
#ifdef HAVE_XMB
         {
            char s1[PATH_MAX_LENGTH];
            char s2[PATH_MAX_LENGTH];
            s1[0]    = '\0';
            fill_pathname_application_special(s1, sizeof(s1),
                  APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);
            fill_pathname_join(s2, s1, "png", sizeof(s2));
            fill_pathname_slash(s2, sizeof(s2));
            strlcpy(s, s2, len);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG:
#ifdef HAVE_XMB
         {
            settings_t *settings            = config_get_ptr();
            const char *path_menu_wallpaper = settings->paths.path_menu_wallpaper;

            if (!string_is_empty(path_menu_wallpaper))
               strlcpy(s, path_menu_wallpaper, len);
            else
            {
               char s3[PATH_MAX_LENGTH];

               s3[0] = '\0';

               fill_pathname_application_special(s3, sizeof(s3),
                     APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);
               fill_pathname_join(s, s3, FILE_PATH_BACKGROUND_IMAGE, len);
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS:
         {
#ifdef HAVE_MENU
            char s4[PATH_MAX_LENGTH];
            settings_t *settings   = config_get_ptr();
            const char *menu_ident = settings->arrays.menu_driver;
            const char *dir_assets = settings->paths.directory_assets;

            s4[0]                  = '\0';

            if (string_is_equal(menu_ident, "xmb"))
            {
               fill_pathname_application_special(s4, sizeof(s4),
                     APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);

               if (!string_is_empty(s4))
                  strlcat(s4, "/sounds", sizeof(s4));
            }
            else if (string_is_equal(menu_ident, "glui"))
            {
               const char *dir_assets = settings->paths.directory_assets;
               fill_pathname_join(s4, dir_assets, "glui", sizeof(s4));

               if (!string_is_empty(s4))
                  strlcat(s4, "/sounds", sizeof(s4));
            }
            else if (string_is_equal(menu_ident, "ozone"))
            {
               const char *dir_assets   = settings->paths.directory_assets;
               fill_pathname_join(s4, dir_assets, "ozone",
                     sizeof(s4));

               if (!string_is_empty(s4))
                  strlcat(s4, "/sounds", sizeof(s4));
            }

            if (string_is_empty(s4))
               fill_pathname_join(
                     s4, dir_assets, "sounds", sizeof(s4));

            strlcpy(s, s4, len);
#endif
         }

         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_SYSICONS:
         {
#ifdef HAVE_MENU
            settings_t *settings   = config_get_ptr();
            const char *menu_ident = settings->arrays.menu_driver;

            if      (string_is_equal(menu_ident, "xmb"))
               fill_pathname_application_special(s, len, APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);
            else if (    string_is_equal(menu_ident, "ozone")
                      || string_is_equal(menu_ident, "glui"))
               fill_pathname_application_special(s, len, APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS);
            else if (len)
               s[0] = '\0';
#endif
         }

         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_OZONE_ICONS:
#ifdef HAVE_OZONE
         {
            char s5[PATH_MAX_LENGTH];
            char s6[PATH_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;

#if defined(WIIU) || defined(VITA)
            /* Smaller 46x46 icons look better on low-DPI devices */
            fill_pathname_join(s5, dir_assets, "ozone", sizeof(s5));
            fill_pathname_join(s6, "png", "icons", sizeof(s6));
#else
            /* Otherwise, use large 256x256 icons */
            fill_pathname_join(s5, dir_assets, "xmb", sizeof(s5));
            fill_pathname_join(s6, "monochrome", "png", sizeof(s6));
#endif
            fill_pathname_join(s, s5, s6, len);
         }
#endif
         break;

      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_RGUI_FONT:
#ifdef HAVE_RGUI
         {
            char s7[PATH_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;
            fill_pathname_join(s7, dir_assets, "rgui", sizeof(s7));
            fill_pathname_join(s, s7, "font", len);
         }
#endif
         break;

      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB:
#ifdef HAVE_XMB
         {
            char s8[PATH_MAX_LENGTH];
            settings_t *settings     = config_get_ptr();
            const char *dir_assets   = settings->paths.directory_assets;
            fill_pathname_join(s8, dir_assets, "xmb", sizeof(s8));
            fill_pathname_join(s, s8, xmb_theme_ident(), len);
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT:
#ifdef HAVE_XMB
         {
            settings_t           *settings = config_get_ptr();
            const char *path_menu_xmb_font = settings->paths.path_menu_xmb_font;

            if (!string_is_empty(path_menu_xmb_font))
               strlcpy(s, path_menu_xmb_font, len);
            else
            {
               char s9[PATH_MAX_LENGTH];
               s9[0] = '\0';

               switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
               {
                  case RETRO_LANGUAGE_ARABIC:
                  case RETRO_LANGUAGE_PERSIAN:
                     fill_pathname_join(s9,
                           settings->paths.directory_assets, "pkg", sizeof(s9));
                     fill_pathname_join(s, s9, "fallback-font.ttf", len);
                     break;
                  case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
                  case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
                     fill_pathname_join(s9,
                           settings->paths.directory_assets, "pkg", sizeof(s9));
                     fill_pathname_join(s, s9, "chinese-fallback-font.ttf", len);
                     break;
                  case RETRO_LANGUAGE_KOREAN:
                     fill_pathname_join(s9,
                           settings->paths.directory_assets, "pkg", sizeof(s9));
                     fill_pathname_join(s, s9, "korean-fallback-font.ttf", len);
                     break;
                  default:
                     fill_pathname_application_special(s9, sizeof(s9),
                           APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB);
                     fill_pathname_join(s, s9, FILE_PATH_TTF_FONT, len);
               }
            }
         }
#endif
         break;
      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS:
      {
        char s10[PATH_MAX_LENGTH];
        char s11[PATH_MAX_LENGTH];
        settings_t *settings       = config_get_ptr();
        const char *dir_thumbnails = settings->paths.directory_thumbnails;
        fill_pathname_join(s10, dir_thumbnails, "discord", sizeof(s10));
        fill_pathname_join(s11, s10, "avatars", sizeof(s11));
        fill_pathname_slash(s11, sizeof(s11));
        strlcpy(s, s11, len);
      }
      break;

      case APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES:
      {
        char s12[PATH_MAX_LENGTH];
        char s13[PATH_MAX_LENGTH];
        settings_t *settings       = config_get_ptr();
        const char *dir_thumbnails = settings->paths.directory_thumbnails;
        fill_pathname_join(s12, dir_thumbnails, "cheevos", len);
        fill_pathname_join(s13, s12, "badges", sizeof(s13));
        fill_pathname_slash(s13, sizeof(s13));
        strlcpy(s, s13, len);
      }
      break;

      case APPLICATION_SPECIAL_NONE:
      default:
         break;
   }
}
