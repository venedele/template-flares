/* plugin.c - boot splash plugin
 *
 * Copyright (C) 2007, 2008 Red Hat, Inc.
 *                     2008 Charlie Brej <cbrej@cs.man.ac.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 *             Ray Strode <rstrode@redhat.com>
 */

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

#include "plugin_base.h"

static view_t *
view_new (ply_boot_splash_plugin_t *plugin,
          ply_pixel_display_t      *display)
{
        view_t *view;

        view = calloc (1, sizeof(view_t));
        view->plugin = plugin;
        view->display = display;
        
        view->sprites = ply_list_new ();

        if (ply_console_viewer_preferred ()) {
                view->console_viewer = ply_console_viewer_new (view->display, plugin->monospace_font);
                ply_console_viewer_set_text_color (view->console_viewer, plugin->console_text_color);

                if (plugin->boot_buffer)
                        ply_console_viewer_convert_boot_buffer (view->console_viewer, plugin->boot_buffer);
        } else {
                view->console_viewer = NULL;
        }
        
        if (plugin->plugin_base_interface->view_create_data != NULL)
        {
                view->data = plugin->plugin_base_interface->view_create_data(view);
        } else {
                view->data = NULL;
        }

        return view;
}

static void
view_free (view_t *view)
{
        ply_boot_splash_plugin_t* plugin = view->plugin;
        if (plugin->plugin_base_interface->view_free_data != NULL && view->data != NULL)
        {
               plugin->plugin_base_interface->view_free_data(view);
        }
        
        view_free_sprites (view);
        ply_list_free (view->sprites);

        ply_console_viewer_free (view->console_viewer);

        ply_image_free (view->scaled_background_image);

        free (view);
}

static void
free_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_free (view);

                node = next_node;
        }

        ply_list_free (plugin->views);
        plugin->views = NULL;
}

static bool
view_load (view_t *view)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;
        if (plugin->plugin_base_interface->view_load != NULL)
                return plugin->plugin_base_interface->view_load(view);
        return true;
}

//NOTE: Assumes that load view will always suceed if there is no callback defined
static bool
load_views (ply_boot_splash_plugin_t *plugin)
{
        if (plugin->plugin_base_interface->view_load != NULL)
        {
                ply_list_node_t *node;
                bool view_loaded;
                
                view_loaded = false;
                node = ply_list_get_first_node (plugin->views);
                
                while (node != NULL) {
                        ply_list_node_t *next_node;
                        view_t *view;
                
                        view = ply_list_node_get_data (node);
                        next_node = ply_list_get_next_node (plugin->views, node);
                
                        if (view_load (view))
                                view_loaded = true;
                
                        node = next_node;
                }
                
                return view_loaded;
        }
        return true;
}

static void
view_redraw (view_t *view)
{
        unsigned long screen_width, screen_height;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        ply_pixel_display_draw_area (view->display, 0, 0,
                                     screen_width, screen_height);
}

static void
redraw_views (ply_boot_splash_plugin_t *plugin)
{
        plugin->needs_redraw = true;
}

static void
process_needed_redraws (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        if (!plugin->needs_redraw)
                return;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_redraw (view);

                node = next_node;
        }

        plugin->needs_redraw = false;
}

static void pause_views_animation(ply_boot_splash_plugin_t* plugin)
{
        if (plugin->loop != NULL) {
                ply_event_loop_stop_watching_for_timeout (plugin->loop,
                                                (ply_event_loop_timeout_handler_t)
                                                on_timeout, plugin);
                plugin->is_updating_animation = false;
        }
}

static void
pause_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                ply_pixel_display_pause_updates (view->display);

                node = next_node;
        }
}

static void unpause_views_animation(ply_boot_splash_plugin_t* plugin)
{
        on_timeout(plugin);
}

static void
unpause_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                ply_pixel_display_unpause_updates (view->display);

                node = next_node;
        }
}

sprite_t *
add_sprite (view_t      *view,
            ply_image_t *image,
            void        *data)
{
        sprite_t *new_sprite = calloc (1, sizeof(sprite_t));

        new_sprite->x = 0;
        new_sprite->y = 0;
        new_sprite->z = 0;
        new_sprite->oldx = 0;
        new_sprite->oldy = 0;
        new_sprite->oldz = 0;
        new_sprite->opacity = 1;
        new_sprite->refresh_me = 0;
        new_sprite->image = image;
        new_sprite->data = data;
        new_sprite->custom_draw = NULL;
        new_sprite->custom_object = NULL;

        ply_list_append_data (view->sprites, new_sprite);

        return new_sprite;
}

sprite_t* add_custom_sprite(view_t* view,
                            void* custom_object,
                            sprite_custom_drawer_t* custom_draw_handle,
                            void* data)
{
        sprite_t *new_sprite = calloc (1, sizeof(sprite_t));

        new_sprite->x = 0;
        new_sprite->y = 0;
        new_sprite->z = 0;
        new_sprite->oldx = 0;
        new_sprite->oldy = 0;
        new_sprite->oldz = 0;
        new_sprite->opacity = 1;
        new_sprite->refresh_me = 0;
        new_sprite->image = NULL;
        new_sprite->data = data;
        new_sprite->custom_draw = custom_draw_handle;
        new_sprite->custom_object = custom_object;

        ply_list_append_data (view->sprites, new_sprite);

        return new_sprite;
}

static void
view_show_prompt_on_console_viewer (view_t     *view,
                                    const char *prompt,
                                    const char *entry_text,
                                    int         number_of_bullets)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;

        if (view->console_viewer == NULL)
                return;

        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                ply_console_viewer_print (view->console_viewer, "\n");

        ply_console_viewer_clear_line (view->console_viewer);

        ply_console_viewer_print (view->console_viewer, prompt);

        ply_console_viewer_print (view->console_viewer, ": ");
        if (entry_text)
                ply_console_viewer_print (view->console_viewer, "%s", entry_text);

        for (int i = 0; i < number_of_bullets; i++) {
                ply_console_viewer_print (view->console_viewer, " ");
        }

        ply_console_viewer_print (view->console_viewer, "_");
}

static void
view_show_prompt (view_t     *view,
                  const char *prompt,
                  const char *entry_text,
                  int         number_of_bullets)
{
        ply_boot_splash_plugin_t* plugin;
        assert (view != NULL);
        plugin = view->plugin;
        if (plugin->plugin_base_interface->view_show_prompt != NULL)
        {
                plugin->plugin_base_interface->view_show_prompt(view, prompt, entry_text, number_of_bullets);
                view_update_dirty_regions(view);
        }
}

//NOTE: As console viewers are not really the focus of this plugin base no view_show_message_on_console_viewer is implemented. This can be done in the future if console message callbacks are added.
static void
view_show_message (view_t     *view,
                  const char *message)
{
        ply_boot_splash_plugin_t* plugin;
        assert (view != NULL);
        plugin = view->plugin;
        if (plugin->plugin_base_interface->view_show_message != NULL)
        {
                plugin->plugin_base_interface->view_show_message(view, message);
                view_update_dirty_regions(view);
        }
}

static void
view_hide_prompt (view_t *view)
{
        ply_boot_splash_plugin_t *plugin;

        assert (view != NULL);

        plugin = view->plugin;

        /* Obscure the password length in the scroll back */
        if (view->console_viewer != NULL) {
                if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY)
                        ply_console_viewer_clear_line (view->console_viewer);

                ply_console_viewer_print (view->console_viewer, "\n");
        }

        if (plugin->plugin_base_interface->view_hide_prompt != NULL)
        {
                plugin->plugin_base_interface->view_hide_prompt(view);
                view_update_dirty_regions(view);
        }
                
}

static void
hide_prompt (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_hide_prompt (view);

                node = next_node;
        }
}


static int
sprite_compare_z (void *data_a,
                  void *data_b)
{
        sprite_t *sprite_a = data_a;
        sprite_t *sprite_b = data_b;

        return sprite_a->z - sprite_b->z;
}

void
stretch_image (ply_image_t *scaled_image,
               ply_image_t *orig_image,
               int          width)
{
        int x, y;
        int stretched_width = ply_image_get_width (scaled_image);
        int stretched_height = ply_image_get_height (scaled_image);
        int orig_width = ply_image_get_width (orig_image);
        uint32_t *scaled_image_data = ply_image_get_data (scaled_image);
        uint32_t *orig_image_data = ply_image_get_data (orig_image);


        for (y = 0; y < stretched_height; y++) {
                float my_width = y + 0.5;
                my_width /= (stretched_height);
                my_width *= 2;
                my_width -= 1;
                my_width *= my_width;
                my_width = sqrt (1 - my_width) - 1;
                my_width *= stretched_height;
                my_width /= 2;
                my_width = width + my_width;
                for (x = 0; x < stretched_width; x++) {
                        if (x < my_width) {
                                uint32_t value = 0x0;
                                int new_x = (x * orig_width) / width;
                                value = orig_image_data[new_x + y * orig_width];
                                scaled_image_data[x + y * stretched_width] = value;
                        } else {
                                scaled_image_data[x + y * stretched_width] = 0;
                        }
                }
        }
}

void
sprite_list_sort (view_t *view)
{
        ply_list_sort_stable (view->sprites, &sprite_compare_z);
}

static void
on_timeout (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        double sleep_time;
        double now;

        now = ply_get_timestamp ();
        plugin->is_updating_animation = true;

        node = ply_list_get_first_node (plugin->views);

        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_animate_attime (view, now);

                node = next_node;
        }
        plugin->now = now;

        sleep_time = 1.0 / FRAMES_PER_SECOND;

        ply_event_loop_watch_for_timeout (plugin->loop,
                                          sleep_time,
                                          (ply_event_loop_timeout_handler_t)
                                          on_timeout, plugin);
}

static void
on_boot_progress (ply_boot_splash_plugin_t *plugin,
                  double                    duration,
                  double                    fraction_done)
{
        //NOTE: I don't think this code is correct
        if (plugin->progress_target < 0)
                plugin->progress = fraction_done;
        plugin->progress_target = fraction_done;
        
        if (plugin->plugin_base_interface->on_boot_progress != NULL)
        {
                plugin->plugin_base_interface->on_boot_progress(plugin, duration, fraction_done);
        }
        
}

static void
start_animation (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        if (plugin->is_animating)
                return;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                if(!view->initialized)
                        view_start_animation (view);

                node = next_node;
        }

        if(!plugin->is_updating_animation)
                on_timeout (plugin);

        plugin->is_animating = true;
}

static void
view_free_sprites (view_t *view)
{
        ply_list_node_t *node;

        for (node = ply_list_get_first_node (view->sprites);
             node != NULL;
             node = ply_list_get_next_node (view->sprites, node)) {
                sprite_t *sprite = ply_list_node_get_data (node);
                free_sprite (sprite, view->plugin);
        }
        ply_list_remove_all_nodes (view->sprites);
        view->initialized = false;
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
        plugin->loop = NULL;
}

static void
on_draw (view_t             *view,
         ply_pixel_buffer_t *pixel_buffer,
         int                 x,
         int                 y,
         int                 width,
         int                 height)
{
        ply_boot_splash_plugin_t *plugin;
        ply_rectangle_t clip_area;

        clip_area.x = x;
        clip_area.y = y;
        clip_area.width = width;
        clip_area.height = height;

        bool single_pixel = 0;
        float pixel_r = 0;
        float pixel_g = 0;
        float pixel_b = 0;

        plugin = view->plugin;

        if (width == 1 && height == 1)
                single_pixel = true;

        if (!plugin->should_show_console_messages) {
                ply_list_node_t *node;


                for (node = ply_list_get_first_node (view->sprites); node; node = ply_list_get_next_node (view->sprites, node)) {
                        sprite_t *sprite = ply_list_node_get_data (node);
                        ply_rectangle_t sprite_area;
                        sprite_custom_drawer_t* draw_obj = sprite->custom_draw;


                        sprite_area.x = sprite->x;
                        sprite_area.y = sprite->y;

                        if (sprite_area.x >= (x + width)) continue;
                        if (sprite_area.y >= (y + height)) continue;

                        //TODO: Perhaps include sprite_area inside of sprite_t instead of recalculating. A resize_sprite to image function can be added.
                        if(draw_obj != NULL) {
                                sprite_area.width = width;
                                sprite_area.height = height;
                                sprite_area.x = x;
                                sprite_area.y = y;
                        }
                        else
                        {
                                sprite_area.width = ply_image_get_width (sprite->image);
                                sprite_area.height = ply_image_get_height (sprite->image);
                        }

                        if ((int) (sprite_area.x + sprite_area.width) <= x) continue;
                        if ((int) (sprite_area.y + sprite_area.height) <= y) continue;

                        if (draw_obj != NULL)
                        {
                                draw_obj->draw_area_handle(sprite->custom_object, pixel_buffer, sprite_area.x, sprite_area.y, sprite_area.width, sprite_area.height);
                        }
                        else
                        {
                                if (single_pixel) {
                                        uint32_t *image_data = ply_image_get_data (sprite->image);
                                        uint32_t overlay_pixel = image_data[(x - sprite_area.x) + (y - sprite_area.y) * sprite_area.width];
                                        float alpha = (float) ((overlay_pixel >> 24) & 0xff) / 255 * sprite->opacity;
                                        float red = (float) ((overlay_pixel >> 16) & 0xff) / 255 * sprite->opacity;
                                        float green = (float) ((overlay_pixel >> 8) & 0xff) / 255 * sprite->opacity;
                                        float blue = (float) ((overlay_pixel >> 0) & 0xff) / 255 * sprite->opacity;
                                        pixel_r = pixel_r * (1 - alpha) + red;
                                        pixel_g = pixel_g * (1 - alpha) + green;
                                        pixel_b = pixel_b * (1 - alpha) + blue;
                                } else {
                                        ply_pixel_buffer_fill_with_argb32_data_at_opacity_with_clip (pixel_buffer,
                                                                                             &sprite_area, &clip_area,
                                                                                             ply_image_get_data (sprite->image), sprite->opacity);
                                }
                        }
                }
        }
        if (single_pixel)
                ply_pixel_buffer_fill_with_color (pixel_buffer, &clip_area, pixel_r, pixel_g, pixel_b, 1.0);

        if (!plugin->plugin_console_messages_updating && view->console_viewer != NULL)
                ply_console_viewer_draw_area (view->console_viewer, pixel_buffer, x, y, width, height);
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
        view_t *view;

        view = view_new (plugin, display);

        ply_pixel_display_set_draw_handler (view->display,
                                            (ply_pixel_display_draw_handler_t)
                                            on_draw, view);
        
        if (plugin->is_visible) {
                if (view_load (view)) {
                        ply_list_append_data (plugin->views, view);
                }
                else {
                        view_free (view);
                        view = NULL;
                }
        } else {
                ply_list_append_data (plugin->views, view);
        }
        if (plugin->plugin_base_interface->add_pixel_display != NULL)
        {
                plugin->plugin_base_interface->add_pixel_display(plugin, display, view);
        }
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view_t *view;
                ply_list_node_t *next_node;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                if (view->display == display) {
                        ply_pixel_display_set_draw_handler (view->display, NULL, NULL);
                        view_free (view);
                        ply_list_remove_node (plugin->views, node);
                        
                        
                        
                        if (plugin->plugin_base_interface->remove_pixel_display != NULL)
                        {
                                plugin->plugin_base_interface->remove_pixel_display(plugin, display);
                        }
                        
                        return;
                }

                node = next_node;
        }
        if (plugin->plugin_base_interface->remove_pixel_display != NULL)
        {
                plugin->plugin_base_interface->remove_pixel_display(plugin, display);
        }
        
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
        assert (plugin != NULL);
        
        if (plugin->plugin_base_interface->update_status != NULL)
        {
                plugin->plugin_base_interface->update_status(plugin, status);
        }
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
        assert (plugin != NULL);

        if (plugin->loop != NULL) {
                stop_animation (plugin);

                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }
        
        
        if (plugin->plugin_base_interface->hide_splash_screen != NULL)
        {
                plugin->plugin_base_interface->hide_splash_screen(plugin);
        }

        plugin->is_visible = false;
}

static void
show_password_prompt (ply_boot_splash_plugin_t *plugin,
                      const char               *text,
                      int                       number_of_bullets)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_show_prompt_on_console_viewer (view, text, NULL, number_of_bullets);
                view_show_prompt (view, text, NULL, number_of_bullets);

                node = next_node;
        }
}

static void
show_prompt (ply_boot_splash_plugin_t *plugin,
             const char               *prompt,
             const char               *entry_text)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_show_prompt_on_console_viewer (view, prompt, entry_text, -1);
                view_show_prompt (view, prompt, entry_text, -1);

                node = next_node;
        }
}

static void
on_root_mounted (ply_boot_splash_plugin_t *plugin)
{
        plugin->root_is_mounted = true;
        
        if (plugin->plugin_base_interface->on_root_mounted != NULL)
        {
                plugin->plugin_base_interface->on_root_mounted(plugin);
        }
}

static void
become_idle (ply_boot_splash_plugin_t *plugin,
             ply_trigger_t            *idle_trigger)
{
        stop_animation (plugin);
        ply_trigger_pull (idle_trigger, NULL);
        
        if (plugin->plugin_base_interface->become_idle != NULL)
        {
                plugin->plugin_base_interface->become_idle(plugin, idle_trigger);
        }
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
        pause_views (plugin);
        if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL)
        {
                hide_prompt (plugin);
                unpause_views_animation(plugin);
        }

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
        
        
        if (plugin->plugin_base_interface->display_normal != NULL)
        {
                plugin->plugin_base_interface->display_normal(plugin);
        }
        
        if (!plugin->should_show_console_messages)
                start_animation (plugin);

        redraw_views (plugin);

        if (plugin->should_show_console_messages)
                display_console_messages (plugin);

        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        pause_views (plugin);
        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                //stop_animation (plugin);
                //TODO: Leave this to the user to decide. Animations might be desired to keep running
                pause_views_animation(plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;
        
        if (plugin->plugin_base_interface->display_password != NULL)
        {
                plugin->plugin_base_interface->display_password(plugin, prompt, bullets);
        }
        
        show_password_prompt (plugin, prompt, bullets);
        redraw_views (plugin);

        if (plugin->should_show_console_messages)
                display_console_messages (plugin);

        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        pause_views (plugin);
        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                //stop_animation (plugin);
                pause_views_animation(plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY;
        
        if (plugin->plugin_base_interface->display_question != NULL)
        {
                plugin->plugin_base_interface->display_question(plugin, prompt, entry_text);
        }
        
        show_prompt (plugin, prompt, entry_text);
        redraw_views (plugin);

        if (plugin->should_show_console_messages)
                display_console_messages (plugin);

        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
        if (plugin->plugin_base_interface->display_message != NULL)
        {
                plugin->plugin_base_interface->display_message(plugin, message);
        }
        
        show_message (plugin, message);
}

static void
show_message (ply_boot_splash_plugin_t *plugin,
              const char               *message)
{
        ply_trace ("Showing message '%s'", message);
        ply_list_node_t *node;
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);
                
                view_show_message(view, message);
                
                if (view->console_viewer != NULL)
                        ply_console_viewer_print (view->console_viewer, "\n%s\n", message);

                node = next_node;
        }
}
static bool
validate_input (ply_boot_splash_plugin_t *plugin,
                const char               *entry_text,
                const char               *add_text)
{
        if (!ply_console_viewer_preferred ())
                return true;

        if (strcmp (add_text, "\e") == 0) {
                toggle_console_messages (plugin);
                return false;
        }

        return true;
}

static void
toggle_console_messages (ply_boot_splash_plugin_t *plugin)
{
        if (plugin->should_show_console_messages) {
                plugin->should_show_console_messages = false;
                hide_console_messages (plugin);
        } else {
                plugin->should_show_console_messages = true;
                display_console_messages (plugin);
        }
}

static void
display_console_messages (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        pause_views (plugin);

        if (plugin->should_show_console_messages)
                stop_animation (plugin);

        plugin->plugin_console_messages_updating = true;
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                if (view->console_viewer != NULL)
                        ply_console_viewer_show (view->console_viewer, view->display);
                node = ply_list_get_next_node (plugin->views, node);
        }
        plugin->plugin_console_messages_updating = false;

        redraw_views (plugin);
        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
hide_console_messages (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        plugin->should_show_console_messages = false;

        pause_views (plugin);
        plugin->plugin_console_messages_updating = true;
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                ply_console_viewer_hide (view->console_viewer);
                node = ply_list_get_next_node (plugin->views, node);
        }
        plugin->plugin_console_messages_updating = false;
        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                start_animation (plugin);

        redraw_views (plugin);
        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
on_boot_output (ply_boot_splash_plugin_t *plugin,
                const char               *output,
                size_t                    size)
{
        ply_list_node_t *node;
        view_t *view;

        if (!ply_console_viewer_preferred ())
                return;
        
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                ply_console_viewer_write (view->console_viewer, output, size);
                node = ply_list_get_next_node (plugin->views, node);
        }
        
        
        if (plugin->plugin_base_interface->on_boot_output != NULL)
        {
                plugin->plugin_base_interface->on_boot_output(plugin, output, size);
        }
        
}



static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
        ply_boot_splash_plugin_t *plugin;
        char *image_dir;

        plugin = calloc (1, sizeof(ply_boot_splash_plugin_t));

        //plugin->logo_image = ply_image_new (PLYMOUTH_LOGO_FILE);

        image_dir = ply_key_file_get_value (key_file, PLUGIN_NAME, "ImageDir");

        
        
        plugin->plugin_console_messages_updating = false;
        plugin->should_show_console_messages = false;

        /* Likely only able to set the font if the font is in the initrd */
        plugin->monospace_font = ply_key_file_get_value (key_file, PLUGIN_NAME, "MonospaceFont");

        if (plugin->monospace_font == NULL)
                plugin->monospace_font = strdup ("monospace 10");

        plugin->console_text_color =
                ply_key_file_get_ulong (key_file, PLUGIN_NAME,
                                        "ConsoleLogTextColor",
                                        PLY_CONSOLE_VIEWER_LOG_TEXT_COLOR);

        plugin->console_background_color =
                ply_key_file_get_ulong (key_file, PLUGIN_NAME,
                                        "ConsoleLogBackgroundColor",
                                        0x00000000);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
        plugin->progress = 0;
        plugin->progress_target = -1;

        plugin->image_dir = image_dir;

        plugin->views = ply_list_new ();

        plugin->plugin_base_interface = create_base_interface();
        //TODO: Remove create plugin data eventually. Should be handled by create_plugin or alternatively be made a callback
        if (plugin->plugin_base_interface->create_plugin_data != NULL)
        {
                plugin->plugin_data = plugin->plugin_base_interface->create_plugin_data(plugin, key_file);
        } else {
                plugin->plugin_data = NULL;
        }
        
        plugin->needs_redraw = true;
        
        
        return plugin;
}



static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
        if (plugin == NULL)
                return;

        free (plugin->image_dir);

        if (plugin->loop != NULL) {
                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);

                detach_from_event_loop (plugin);
        }


        free (plugin->monospace_font);

        free_views (plugin);
        
        if (plugin->plugin_base_interface->destroy_plugin_data != NULL)
        {
                plugin->plugin_base_interface->destroy_plugin_data(plugin, plugin->plugin_data);
        }

        free (plugin);
}



static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
        ply_list_node_t *node;

        assert (plugin != NULL);

        plugin->loop = loop;
        plugin->mode = mode;

        if (boot_buffer && ply_console_viewer_preferred ()) {
                plugin->boot_buffer = boot_buffer;

                node = ply_list_get_first_node (plugin->views);
                while (node != NULL) {
                        view_t *view;
                        ply_list_node_t *next_node;

                        view = ply_list_node_get_data (node);
                        next_node = ply_list_get_next_node (plugin->views, node);

                        if (view->console_viewer != NULL)
                                ply_console_viewer_convert_boot_buffer (view->console_viewer, plugin->boot_buffer);

                        node = next_node;
                }
        }

        if (!load_views (plugin)) {
                ply_trace ("couldn't load views");
                return false;
        }
        
        
        bool res = true;
        if (plugin->plugin_base_interface->show_splash_screen != NULL)
        {
                
                res = plugin->plugin_base_interface->show_splash_screen(plugin, mode);
                if(!res) return res;
        }
        
        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       plugin);

        ply_trace ("starting boot animation");
        
        start_animation (plugin);

        plugin->is_visible = true;

        return res;
}

void view_update_dirty_regions(view_t* view)
{
        ply_list_node_t *node;
        sprite_list_sort (view);

        for (node = ply_list_get_first_node (view->sprites); node; node = ply_list_get_next_node (view->sprites, node)) {
                sprite_t *sprite = ply_list_node_get_data (node);
                if (sprite->x != sprite->oldx ||
                    sprite->y != sprite->oldy ||
                    sprite->z != sprite->oldz ||
                    sprite->refresh_me) {
                        sprite->refresh_me = 0;
                        sprite_custom_drawer_t* draw_obj = sprite->custom_draw;
                        //TODO: Add a changed width detection


                        int width, height;
                        if(draw_obj != NULL) {
                                width = draw_obj->get_width(sprite->custom_object);
                                height = draw_obj->get_height(sprite->custom_object);
                                if(sprite->opacity > 0)
                                        draw_obj->move_handle(sprite->custom_object, view, sprite->x, sprite->y);
                                //NOTE: In the case of ply_entry draw_area will be called twice, but it will be the same region, so no extra work will be done.
                        } else {
                                width = ply_image_get_width (sprite->image);
                                height = ply_image_get_height (sprite->image);
                        }

                        int x = sprite->x - sprite->oldx;
                        int y = sprite->y - sprite->oldy;

                        if (x < width && x > -width && y < height && y > -height) {
                                x = MIN (sprite->x, sprite->oldx);
                                y = MIN (sprite->y, sprite->oldy);
                                width = (MAX (sprite->x, sprite->oldx) - x) + width;
                                height = (MAX (sprite->y, sprite->oldy) - y) + height;
                                ply_pixel_display_draw_area (view->display,
                                                             x, y, width, height);
                        } else {
                                ply_pixel_display_draw_area (view->display,
                                                             sprite->x, sprite->y, width, height);
                                ply_pixel_display_draw_area (view->display,
                                                             sprite->oldx, sprite->oldy, width, height);
                        }
                        sprite->oldx = sprite->x;
                        sprite->oldy = sprite->y;
                        sprite->oldz = sprite->z;
                }
        } 
}

static void
view_animate_attime (view_t *view,
                     double  time)
{
        ply_boot_splash_plugin_t *plugin;

        plugin = view->plugin;

        if (plugin->progress_target >= 0)
                plugin->progress = (plugin->progress * 10 + plugin->progress_target) / 11;
        
        
        if (plugin->plugin_base_interface->view_animate_attime != NULL)
        {
                plugin->plugin_base_interface->view_animate_attime(view, time);
        }
        
        
        view_update_dirty_regions(view);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        assert (plugin != NULL);
        assert (plugin->loop != NULL);

        if (!plugin->is_animating)
                return;

        plugin->is_animating = false;
        plugin->is_updating_animation = false;

        pause_views_animation(plugin);

        
        if (plugin->plugin_base_interface->stop_animation != NULL)
        {
                plugin->plugin_base_interface->stop_animation(plugin);
        }
        
        for (node = ply_list_get_first_node (plugin->views); node; node = ply_list_get_next_node (plugin->views, node)) {
                view_t *view = ply_list_node_get_data (node);
                view_free_sprites (view);
        }
}

static void
view_start_animation (view_t *view)
{
        unsigned long screen_width, screen_height;

        assert (view != NULL);

        view_setup_scene (view);

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        ply_pixel_display_draw_area (view->display, 0, 0,
                                     screen_width, screen_height);
        
        
        if (view->plugin->plugin_base_interface->view_start_animation != NULL)
        {
                view->plugin->plugin_base_interface->view_start_animation(view);
        }
}

static void
view_setup_scene (view_t *view)
{

        if (view->plugin->plugin_base_interface->view_setup_scene != NULL)
        {
                view->plugin->plugin_base_interface->view_setup_scene(view);
                sprite_list_sort(view);
        }
                
        view->initialized = true;
}

static void
free_sprite (sprite_t *sprite, ply_boot_splash_plugin_t *plugin)
{
        if (sprite) {
                        
                if (plugin->plugin_base_interface->free_sprite_data != NULL && sprite->data != NULL)
                {
                        plugin->plugin_base_interface->free_sprite_data(sprite);
                }
                else
                {
                        if(sprite->data) free(sprite->data);
                }
                free(sprite);
        }
        return;
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
        static ply_boot_splash_plugin_interface_t plugin_interface =
        {
                .create_plugin        = create_plugin,
                .destroy_plugin       = destroy_plugin,
                .add_pixel_display    = add_pixel_display,
                .remove_pixel_display = remove_pixel_display,
                .show_splash_screen   = show_splash_screen,
                .update_status        = update_status,
                .on_boot_progress     = on_boot_progress,
                .hide_splash_screen   = hide_splash_screen,
                .on_root_mounted      = on_root_mounted,
                .become_idle          = become_idle,
                .display_normal       = display_normal,
                .display_password     = display_password,
                .display_question     = display_question,
                .display_message      = display_message,
                .on_boot_output       = on_boot_output,
                .validate_input       = validate_input,
        };

        return &plugin_interface;
}

