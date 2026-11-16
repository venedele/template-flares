#ifndef PLUGIN_BASE_H
#define PLUGIN_BASE_H

#include "plugin_base.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <values.h>
#include <unistd.h>
#include <wchar.h>

#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-entry.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-label.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-trigger.h"
#include "ply-utils.h"
#include "ply-console-viewer.h"

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 40
#endif


#define SHOW_PROGRESS_BAR
#ifndef PLUGIN_NAME
#define PLUGIN_NAME "template"
#endif

typedef enum
{
        PLY_BOOT_SPLASH_DISPLAY_NORMAL,
        PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
        PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

typedef struct
{
        int           x;
        int           y;
        int           z;
        int           oldx;
        int           oldy;
        int           oldz;
        int           refresh_me;
        float         opacity;
        ply_image_t  *image;
        void         *data;
} sprite_t;


typedef struct _plugin_data plugin_data_t;

typedef struct
{
        ply_boot_splash_plugin_t *plugin;
        ply_pixel_display_t      *display;
        ply_list_t               *sprites;
        //TODO: Remove scaled_background_image
        ply_image_t              *scaled_background_image;

        void* data;
        ply_console_viewer_t     *console_viewer;
} view_t;


//TODO: Link all of these
typedef struct
{
        //TODO: Make sure that all duplicate names are static to this file
        //TODO: Delete create plugin, add create_plugin_data as as callback
        plugin_data_t* (*create_plugin_data)(ply_boot_splash_plugin_t *plugin , ply_key_file_t *key_file);
        void (*destroy_plugin_data)(ply_boot_splash_plugin_t *plugin, plugin_data_t* plugin_data);
        void (*set_keyboard)(ply_boot_splash_plugin_t *plugin, ply_keyboard_t *keyboard);
        void (*unset_keyboard)(ply_boot_splash_plugin_t *plugin, ply_keyboard_t *keyboard);
        void (*add_pixel_display)(ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display, view_t *view);
        void (*remove_pixel_display)(ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display);
        void (*add_text_display)(ply_boot_splash_plugin_t *plugin, ply_text_display_t *display);
        void (*remove_text_display)(ply_boot_splash_plugin_t *plugin, ply_text_display_t *display);
        bool (*show_splash_screen)(ply_boot_splash_plugin_t *plugin, ply_boot_splash_mode_t mode);
        void (*system_update)(ply_boot_splash_plugin_t *plugin, int progress);
        void (*update_status)(ply_boot_splash_plugin_t *plugin, const char *status);
        void (*on_boot_output)(ply_boot_splash_plugin_t *plugin, const char *output, size_t size);
        void (*on_boot_progress)(ply_boot_splash_plugin_t *plugin, double duration, double fraction_done);
        void (*on_root_mounted)(ply_boot_splash_plugin_t *plugin);
        void (*hide_splash_screen)(ply_boot_splash_plugin_t *plugin);
        void (*display_message)(ply_boot_splash_plugin_t *plugin, const char *message);
        void (*hide_message)(ply_boot_splash_plugin_t *plugin, const char *message);
        void (*display_normal)(ply_boot_splash_plugin_t *plugin);
        void (*display_password)(ply_boot_splash_plugin_t *plugin, const char *prompt, int bullets);
        void (*display_question)(ply_boot_splash_plugin_t *plugin, const char *prompt, const char *entry_text);
        void (*become_idle)(ply_boot_splash_plugin_t *plugin, ply_trigger_t *idle_trigger);
        void (*display_prompt)(ply_boot_splash_plugin_t *plugin, const char *prompt, const char *entry_text, bool is_secret);
        
        void (*free_sprite_data) (sprite_t *sprite);
        void (*view_animate_attime) (view_t *view, double time);
        void (*view_start_animation) (view_t *view);
        void (*view_setup_scene) (view_t *view);
        void (*stop_animation) (ply_boot_splash_plugin_t *plugin);
        void* (*view_create_data) (view_t *view);
        void (*view_free_data) (view_t *view);
        bool (*view_load) (view_t *view);
        
        void (*view_show_prompt) (view_t *view, const char *prompt, const char *entry_text, int number_of_bullets);
        void (*view_hide_prompt) (view_t *view);
        
        //TODO: Perhaps make the entry and label into sprites
        void (*view_show_message) (view_t *view, const char *message);
        void (*view_hide_message) (view_t *view);
} plugin_base_interface_t;


struct _ply_boot_splash_plugin
{
        plugin_base_interface_t       *plugin_base_interface;
        plugin_data_t                 *plugin_data;
        
        ply_event_loop_t              *loop;
        ply_boot_splash_mode_t         mode;

        char                          *image_dir;
        ply_boot_splash_display_type_t state;
        ply_list_t                    *views;

        double                         now;

        double                         progress;
        double                         progress_target;

        uint32_t                       root_is_mounted : 1;
        uint32_t                       needs_redraw : 1;
        uint32_t                       is_visible : 1;
        uint32_t                       is_animating : 1;

        char                          *monospace_font;
        uint32_t                       plugin_console_messages_updating : 1;
        uint32_t                       should_show_console_messages : 1;
        ply_buffer_t                  *boot_buffer;
        uint32_t                       console_text_color;
        uint32_t                       console_background_color;
};


//TODO: This file should ultimately contain no static functions
plugin_base_interface_t* create_base_interface();


static view_t *view_new (ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display);
static void view_free_sprites (view_t *view);
static void view_free (view_t *view);
static void free_views (ply_boot_splash_plugin_t *plugin);


static bool view_load (view_t *view);
static bool load_views (ply_boot_splash_plugin_t *plugin);
static void view_redraw (view_t *view);
static void redraw_views (ply_boot_splash_plugin_t *plugin);
static void process_needed_redraws (ply_boot_splash_plugin_t *plugin);

static void pause_views (ply_boot_splash_plugin_t *plugin);
static void unpause_views (ply_boot_splash_plugin_t *plugin);

sprite_t *add_sprite (view_t *view, ply_image_t *image, void *data);
static void view_setup_scene (view_t *view);
static void view_start_animation (view_t *view);
static void view_show_prompt_on_console_viewer (view_t *view, const char *prompt, const char *entry_text, int number_of_bullets);

static void view_show_prompt (view_t *view, const char *prompt, const char *entry_text, int number_of_bullets);
static void view_hide_prompt (view_t *view);

static void hide_prompt (ply_boot_splash_plugin_t *plugin);

//TODO: Should figure out where user data is freed. Additionally maybe create_plugin_data is not needed as it is part of create_plugin anyways
static ply_boot_splash_plugin_t *create_plugin (ply_key_file_t *key_file);
static void destroy_plugin (ply_boot_splash_plugin_t *plugin);



static void free_sprite (sprite_t *sprite, ply_boot_splash_plugin_t *plugin);
static int sprite_compare_z (void *data_a, void *data_b);
void stretch_image (ply_image_t *scaled_image, ply_image_t *orig_image, int width);
void sprite_list_sort (view_t *view);

static void on_timeout (ply_boot_splash_plugin_t *plugin);
static void on_boot_progress (ply_boot_splash_plugin_t *plugin, double duration, double fraction_done);


static void start_animation (ply_boot_splash_plugin_t *plugin);
static void view_free_sprites (view_t *view);
static void stop_animation (ply_boot_splash_plugin_t *plugin);

static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);

//NOTE: No callback
static void on_draw (view_t *view, ply_pixel_buffer_t *pixel_buffer, int x, int y, int width, int height);


static void add_pixel_display (ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display);
static void remove_pixel_display (ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display);


static void view_setup_scene (view_t *view);
static bool show_splash_screen (ply_boot_splash_plugin_t *plugin, ply_event_loop_t *loop, ply_buffer_t *boot_buffer, ply_boot_splash_mode_t mode);
static void update_status (ply_boot_splash_plugin_t *plugin, const char *status);
static void hide_splash_screen (ply_boot_splash_plugin_t *plugin, ply_event_loop_t *loop);

static void show_password_prompt (ply_boot_splash_plugin_t *plugin, const char *text, int number_of_bullets);
static void show_prompt (ply_boot_splash_plugin_t *plugin, const char *prompt, const char *entry_text);

static void on_root_mounted (ply_boot_splash_plugin_t *plugin);
static void become_idle (ply_boot_splash_plugin_t *plugin, ply_trigger_t *idle_trigger);

static void view_animate_attime (view_t *view, double time);
static void show_message (ply_boot_splash_plugin_t *plugin, const char *message);
static void display_normal (ply_boot_splash_plugin_t *plugin);
static void display_password (ply_boot_splash_plugin_t *plugin, const char *prompt, int bullets);
static void display_question (ply_boot_splash_plugin_t *plugin, const char *prompt, const char *entry_text);
static void display_message (ply_boot_splash_plugin_t *plugin, const char *message);
static bool validate_input (ply_boot_splash_plugin_t *plugin, const char *entry_text, const char *add_text);
static void toggle_console_messages (ply_boot_splash_plugin_t *plugin);
static void display_console_messages (ply_boot_splash_plugin_t *plugin);
static void hide_console_messages (ply_boot_splash_plugin_t *plugin);
static void on_boot_output (ply_boot_splash_plugin_t *plugin, const char *output, size_t size);

ply_boot_splash_plugin_interface_t * ply_boot_splash_plugin_get_interface (void);

#endif
