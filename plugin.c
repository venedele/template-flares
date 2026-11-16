#include "plugin_base.h"
#include "math.h"



#define FLARE_FRAMES_PER_SECOND 20
#define BG_STARS_FRAMES_PER_SECOND 10
#define FLARE_COUNT 30
#define FLARE_LINE_COUNT 20
#define HALO_BLUR 4
#define STAR_HZ 0.08

/*you can comment one or both of these out*/
/*#define SHOW_PLANETS */
/*#define SHOW_COMETS */
#define SHOW_PROGRESS_BAR
/*#define SHOW_LOGO_HALO */

typedef enum
{
        SPRITE_TYPE_STATIC,
        SPRITE_TYPE_FLARE,
        SPRITE_TYPE_SATELLITE,
        SPRITE_TYPE_PROGRESS,
        SPRITE_TYPE_STAR_BG,
} sprite_type_t;


typedef struct
{
        sprite_type_t type;
        void* dat;
} sprite_data_t;


typedef struct
{
        float        stretch[FLARE_COUNT];
        float        rotate_yz[FLARE_COUNT];
        float        rotate_xy[FLARE_COUNT];
        float        rotate_xz[FLARE_COUNT];
        float        increase_speed[FLARE_COUNT];
        float        z_offset_strength[FLARE_COUNT];
        float        y_size[FLARE_COUNT];
        ply_image_t *image_a;
        ply_image_t *image_b;
        int          frame_count;
} flare_t;

typedef enum
{
        SATELLITE_TYPE_PLANET,
        SATELLITE_TYPE_COMET,
} satellite_type_t;



typedef struct
{
        satellite_type_t type;
        int              start_x;
        int              start_y;
        int              end_x;
        int              end_y;
        int              distance;
        double           theta;
        ply_image_t     *image;
        ply_image_t     *image_altered;
} satellite_t;


typedef struct
{
        int          start_width;
        int          end_width;
        int          current_width;
        ply_image_t *image;
        ply_image_t *image_altered;
} progress_t;


typedef struct
{
        int  star_count;
        int *star_x;
        int *star_y;
        int *star_refresh;
        int  frame_count;
} star_bg_t;

typedef struct {
        ply_entry_t              *entry;
        ply_label_t              *label;
        ply_label_t              *message_label;
        
        
        ply_rectangle_t           box_area, lock_area, logo_area;
        ply_image_t              *scaled_background_image;
} view_data_t;



struct _plugin_data {
        
        ply_image_t                   *logo_image;
        ply_image_t                   *lock_image;
        ply_image_t                   *box_image;
        ply_image_t                   *star_image;

#ifdef  SHOW_PLANETS
        ply_image_t                   *planet_image[5];
#endif
#ifdef  SHOW_PROGRESS_BAR
        ply_image_t                   *progress_barimage;
#endif

#ifdef SHOW_LOGO_HALO
        ply_image_t                   *highlight_logo_image;
#endif
};

//TODO: Message label is perhaps quite broken, some of the code is missing
void* view_create_data(view_t *view) {
        view_data_t* view_data = calloc(1, sizeof(view_data_t));
        ply_boot_splash_plugin_t* plugin = view->plugin;
        
        view_data->entry = ply_entry_new (plugin->image_dir);
        view_data->label = ply_label_new ();
        view_data->message_label = ply_label_new ();
        
        return view_data;
}

void view_free_data(view_t *view){
        view_data_t* view_data = view->data;
        
        ply_entry_free (view_data->entry);
        ply_label_free (view_data->label);
        ply_label_free (view_data->message_label);
        
        free(view_data);
}



static bool
loc_view_load (view_t *view)
{
        ply_trace ("loading entry");
        view_data_t* view_data = view->data;
        if (!ply_entry_load (view_data->entry))
                return false;

        return true;
}


void loc_view_show_prompt(view_t *view, const char *prompt, const char *entry_text, int number_of_bullets){
        
        ply_boot_splash_plugin_t *plugin;
        int x, y;
        int entry_width, entry_height;
        plugin = view->plugin;
        plugin_data_t* plugin_data = plugin->plugin_data;
        view_data_t* view_data = view->data;
        if(number_of_bullets>=0)
        {
                ply_entry_set_bullet_count (view_data->entry, number_of_bullets);
        }
        
        if(entry_text)
        {
                ply_entry_set_text (view_data->entry, entry_text);
        }
        
        
        if (ply_entry_is_hidden (view_data->entry)) {
                unsigned long screen_width, screen_height;

                screen_width = ply_pixel_display_get_width (view->display);
                screen_height = ply_pixel_display_get_height (view->display);

                view_data->box_area.width = ply_image_get_width (plugin_data->box_image);
                view_data->box_area.height = ply_image_get_height (plugin_data->box_image);
                view_data->box_area.x = screen_width / 2.0 - view_data->box_area.width / 2.0;
                view_data->box_area.y = screen_height / 2.0 - view_data->box_area.height / 2.0;

                view_data->lock_area.width = ply_image_get_width (plugin_data->lock_image);
                view_data->lock_area.height = ply_image_get_height (plugin_data->lock_image);

                entry_width = ply_entry_get_width (view_data->entry);
                entry_height = ply_entry_get_height (view_data->entry);

                x = screen_width / 2.0 - (view_data->lock_area.width + entry_width) / 2.0 + view_data->lock_area.width;
                y = screen_height / 2.0 - entry_height / 2.0;

                view_data->lock_area.x = screen_width / 2.0 - (view_data->lock_area.width + entry_width) / 2.0;
                view_data->lock_area.y = screen_height / 2.0 - view_data->lock_area.height / 2.0;

                ply_entry_show (view_data->entry, plugin->loop, view->display, x, y);
        }

        if (prompt != NULL) {
                ply_label_set_text (view_data->label, prompt);

                x = view_data->box_area.x + view_data->lock_area.width / 2;
                y = view_data->box_area.y + view_data->box_area.height;

                ply_label_show (view_data->label, view->display, x, y);
        }
}


void loc_view_show_message(view_t *view, const char *message){
        
        view_data_t* view_data = view->data;
        
        ply_label_set_text (view_data->message_label, message);
        ply_label_show (view_data->message_label, view->display, 10, 10);

        //TODO: Hopefully temporary, can perhaps add labels and entries to sprites
        ply_pixel_display_draw_area (view->display, 10, 10,
                                             ply_label_get_width (view_data->message_label),
                                             ply_label_get_height (view_data->message_label));
}

static void loc_view_hide_prompt(view_t * view)
{
        view_data_t* view_data = view->data;
        ply_entry_hide (view_data->entry);
        ply_label_hide (view_data->label);
}


static plugin_data_t* create_plugin_data(ply_boot_splash_plugin_t * plugin, ply_key_file_t* key_file)
{
        plugin_data_t* plugin_data = calloc (1, sizeof(plugin_data_t));
        
        char *image_dir, *image_path;
        image_dir = plugin->image_dir;//= ply_key_file_get_value (key_file, PLUGIN_NAME, "ImageDir");

        plugin_data->logo_image = ply_image_new (PLYMOUTH_LOGO_FILE);

        asprintf (&image_path, "%s/lock.png", image_dir);
        plugin_data->lock_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/box.png", image_dir);
        plugin_data->box_image = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/star.png", image_dir);
        plugin_data->star_image = ply_image_new (image_path);
        free (image_path);

#ifdef  SHOW_PLANETS

        asprintf (&image_path, "%s/plant1.png", image_dir);
        plugin_data->planet_image[0] = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/plant2.png", image_dir);
        plugin_data->planet_image[1] = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/plant3.png", image_dir);
        plugin_data->planet_image[2] = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/plant4.png", image_dir);
        plugin_data->planet_image[3] = ply_image_new (image_path);
        free (image_path);

        asprintf (&image_path, "%s/plant5.png", image_dir);
        plugin_data->planet_image[4] = ply_image_new (image_path);
        free (image_path);

#endif
#ifdef  SHOW_PROGRESS_BAR

        asprintf (&image_path, "%s/progress_bar.png", image_dir);
        plugin_data->progress_barimage = ply_image_new (image_path);
        free (image_path);
#endif

        return plugin_data;
}



static void destroy_plugin_data(ply_boot_splash_plugin_t *plugin, plugin_data_t* plugin_data)
{
        if(plugin_data == NULL) return;
        
        ply_image_free (plugin_data->logo_image);
        ply_image_free (plugin_data->lock_image);
        ply_image_free (plugin_data->box_image);

        ply_image_free (plugin_data->star_image);
#ifdef  SHOW_PLANETS
        ply_image_free (plugin_data->planet_image[0]);
        ply_image_free (plugin_data->planet_image[1]);
        ply_image_free (plugin_data->planet_image[2]);
        ply_image_free (plugin_data->planet_image[3]);
        ply_image_free (plugin_data->planet_image[4]);
#endif
#ifdef  SHOW_PROGRESS_BAR
        ply_image_free (plugin_data->progress_barimage);
#endif
        
        free(plugin_data);
}


static void free_sprite_data(sprite_t* sprite)
{
        sprite_data_t* sprite_data = sprite->data;
        
        switch (sprite_data->type) {
                case SPRITE_TYPE_STATIC:
                        break;
                case SPRITE_TYPE_SATELLITE:
                {
                        satellite_t *satellite = sprite_data->dat;
                        ply_image_free (satellite->image_altered);
                        break;
                }
                break;
                case SPRITE_TYPE_PROGRESS:
                {
                        progress_t *progress = sprite_data->dat;
                        ply_image_free (progress->image_altered);
                        break;
                }
                break;
                case SPRITE_TYPE_FLARE:
                {
                        flare_t *flare = sprite_data->dat;
                        ply_image_free (flare->image_a);
                        ply_image_free (flare->image_b);
                        break;
                }
                case SPRITE_TYPE_STAR_BG:
                {
                        star_bg_t *star_bg = sprite_data->dat;
                        free (star_bg->star_x);
                        free (star_bg->star_y);
                        free (star_bg->star_refresh);
                        break;
                }
        }

        free (sprite_data);
}


static void
progress_update (view_t   *view,
                 sprite_t *sprite,
                 double    time)
{
        sprite_data_t* sprite_data = sprite->data;
        progress_t *progress = sprite_data->dat;
        ply_boot_splash_plugin_t *plugin = view->plugin;
        int newwidth = plugin->progress * (progress->end_width - progress->start_width) + progress->start_width;

        if (progress->current_width > newwidth) return;
        progress->current_width = newwidth;
        stretch_image (progress->image_altered, progress->image, newwidth);
        sprite->opacity = plugin->progress;
        sprite->refresh_me = 1;
}


static inline uint32_t
star_bg_gradient_colour (int   x,
                         int   y,
                         int   width,
                         int   height,
                         bool  star,
                         float time)
{
        int full_dist = sqrt (width * width + height * height);
        int my_dist = sqrt (x * x + y * y);
        float val;

        uint16_t r0 = 0x0000;  /* start colour:033c73 */
        uint16_t g0 = 0x3c00;
        uint16_t b0 = 0x7300;

        uint16_t r1 = 0x0000;  /* end colour:00193a */
        uint16_t g1 = 0x1900;
        uint16_t b1 = 0x3a00;

        uint16_t r = r0 + ((r1 - r0) * my_dist) / full_dist;
        uint16_t g = g0 + ((g1 - g0) * my_dist) / full_dist;
        uint16_t b = b0 + ((b1 - b0) * my_dist) / full_dist;

        static uint16_t r_err = 0;
        static uint16_t g_err = 0;
        static uint16_t b_err = 0;

        r += r_err;
        g += g_err;
        b += b_err;
        r_err = ((r >> 8) | ((r >> 8) << 8)) - r;
        g_err = ((g >> 8) | ((g >> 8) << 8)) - g;
        b_err = ((b >> 8) | ((b >> 8) << 8)) - b;
        r >>= 8;
        g >>= 8;
        b >>= 8;

        if (!star)

                return 0xff000000 | r << 16 | g << 8 | b;

        x -= width + 720 - 800;
        y -= height + 300 - 480;
        val = sqrt (x * x + y * y) / 100;
        val = (sin (val - time * (2 * M_PI) * STAR_HZ + atan2 (y, x) * 2) + 1) / 2;

        val = val * 0.3;

        r = r * (1 - val) + val * 0xff;
        g = g * (1 - val) + val * 0xff;
        b = b * (1 - val) + val * 0xff;

        return 0xff000000 | r << 16 | g << 8 | b;
}


static void
star_bg_update (view_t   *view,
                sprite_t *sprite,
                double    time)
{
        sprite_data_t* sprite_data = sprite->data;
        star_bg_t *star_bg = sprite_data->dat;
        int width = ply_image_get_width (sprite->image);
        int height = ply_image_get_height (sprite->image);
        uint32_t *image_data = ply_image_get_data (sprite->image);
        int i, x, y;

        star_bg->frame_count++;
        star_bg->frame_count %= FRAMES_PER_SECOND / BG_STARS_FRAMES_PER_SECOND;

        for (i = star_bg->frame_count; i < star_bg->star_count; i += FRAMES_PER_SECOND / BG_STARS_FRAMES_PER_SECOND) {
                x = star_bg->star_x[i];
                y = star_bg->star_y[i];
                uint32_t pixel_colour = star_bg_gradient_colour (x, y, width, height, true, time);
                if (abs ((int) ((image_data[x + y * width] >> 16) & 0xff) - (int) ((pixel_colour >> 16) & 0xff)) > 8) {
                        image_data[x + y * width] = pixel_colour;
                        star_bg->star_refresh[i] = 1;
                }
        }


        sprite->refresh_me = 1;
}



static void
satellite_move (view_t   *view,
                sprite_t *sprite,
                double    time)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;
        sprite_data_t* sprite_data = sprite->data;
        satellite_t *satellite = sprite_data->dat;

        int width = ply_image_get_width (sprite->image);
        int height = ply_image_get_height (sprite->image);
        unsigned long screen_width, screen_height;

        sprite->x = cos (satellite->theta + (1 - plugin->progress) * 2000 / (satellite->distance)) * satellite->distance;
        sprite->y = sin (satellite->theta + (1 - plugin->progress) * 2000 / (satellite->distance)) * satellite->distance;
        sprite->z = 0;

        float distance = sqrt (sprite->z * sprite->z + sprite->y * sprite->y);
        float angle_zy = atan2 (sprite->y, sprite->z) - M_PI * 0.4;

        sprite->z = distance * cos (angle_zy);
        sprite->y = distance * sin (angle_zy);

        float angle_offset = atan2 (sprite->x, sprite->y);
        float cresent_angle = atan2 (sqrt (sprite->x * sprite->x + sprite->y * sprite->y), sprite->z);

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        sprite->x += (float) satellite->end_x * plugin->progress + (float) satellite->start_x * (1 - plugin->progress) - width / 2;
        sprite->y += (float) satellite->end_y * plugin->progress + (float) satellite->start_y * (1 - plugin->progress) - height / 2;

        if (sprite->x > (signed int) screen_width) return;
        if (sprite->y > (signed int) screen_height) return;

        if (satellite->type == SATELLITE_TYPE_PLANET) {
                int x, y;

                uint32_t *image_data = ply_image_get_data (satellite->image);
                uint32_t *cresent_data = ply_image_get_data (satellite->image_altered);

                for (y = 0; y < height; y++) {
                        for (x = 0; x < width; x++) {
                                float fx = x - (float) width / 2;
                                float fy = y - (float) height / 2;
                                float angle = atan2 (fy, fx) + angle_offset;
                                float distance = sqrt (fy * fy + fx * fx);
                                fx = cos (angle) * (distance / (width / 2));
                                fy = sin (angle) * (distance / (height / 2));
                                float want_y = sqrt (1 - fx * fx);
                                want_y *= -cos (cresent_angle);
                                if (fy < want_y) {
                                        cresent_data[x + y * width] = image_data[x + y * width];
                                } else {
                                        int strength = (fy - want_y) * 16 + 2;
                                        uint32_t val = 0;
                                        int alpha = ((image_data[x + y * width] >> 24) & 0xFF);
                                        if (strength <= 0) strength = 1;
                                        if (strength >= 8) strength = 8;
                                        val |= (((image_data[x + y * width] >> 24) & 0xFF) / 1) << 24;
                                        val |= (((image_data[x + y * width] >> 16) & 0xFF) / strength) << 16;
                                        val |= (((image_data[x + y * width] >> 8) & 0xFF) / strength) << 8;
                                        val |= (((image_data[x + y * width] >> 0) & 0xFF) / strength + (alpha - alpha / (strength)) / 8) << 0;
                                        cresent_data[x + y * width] = val;
                                }
                        }
                }
        }

        if (satellite->type == SATELLITE_TYPE_COMET) {
                int x, y;

                uint32_t *image_data = ply_image_get_data (satellite->image);
                uint32_t *comet_data = ply_image_get_data (satellite->image_altered);
                x = width / 2;
                image_data[x] = 0xFFFFFFFF;
                x = 2 * sin (plugin->progress * 62) + width / 2;
                image_data[x] = 0xFFFFFFFF;
                x = 2 * sin (plugin->progress * 163) + width / 2;
                image_data[x] = 0xFFFFFFFF;
                x = 2 * sin (plugin->progress * 275) + width / 2;
                image_data[x] = 0xFFFFFFFF;
                for (y = height - 1; y > 0; y--) {
                        for (x = 1; x < width - 1; x++) {
                                uint32_t pixel;
                                pixel = 2 * (image_data[x + (y - 1) * width] >> 24)
                                        + (image_data[(x - 1) + (y - 1) * width] >> 24)
                                        + (image_data[(x + 1) + (y - 1) * width] >> 24);
                                pixel /= 4.05;
                                pixel |= pixel << 8;
                                pixel |= pixel << 16;
                                image_data[x + y * width] = pixel;
                        }
                }
                for (x = 1; x < width - 1; x++) {
                        image_data[x] = 0x0;
                }
                for (y = 0; y < height; y++) {
                        for (x = 0; x < width; x++) {
                                float scale = cos (M_PI * 0.4);
                                float fx = x;
                                float fy = y;
                                fx -= (float) width / 2;
                                fy -= (float) height / 2;
                                fy /= scale;
                                float angle = atan2 (fy, fx) - (satellite->theta + (1 - plugin->progress) * 2000 / (satellite->distance)) + M_PI / 2 * 0.0;
                                float distance = sqrt (fy * fy + fx * fx);
                                fx = cos (angle) * distance;
                                fy = sin (angle) * distance;
                                fx += (fy * fy * 2) / (satellite->distance);
                                fx += (float) width / 2;
                                fy += (float) height / 2;
                                int ix = fx;
                                int iy = fy;
                                if (ix < 0 || iy < 0 || ix >= width || iy >= height)
                                        comet_data[x + y * width] = 0;
                                else
                                        comet_data[x + y * width] = image_data[ix + iy * width];
                        }
                }
        }
        return;
}



static void
flare_reset (flare_t *flare,
             int      index)
{
        flare->rotate_yz[index] = ((float) (ply_get_random_number (0, 1000)) / 1000) * 2 * M_PI;
        flare->rotate_xy[index] = ((float) (ply_get_random_number (0, 1000)) / 1000) * 2 * M_PI;
        flare->rotate_xz[index] = ((float) (ply_get_random_number (0, 1000)) / 1000) * 2 * M_PI;
        flare->y_size[index] = ((float) (ply_get_random_number (0, 1000)) / 1000) * 0.8 + 0.2;
        flare->increase_speed[index] = ((float) (ply_get_random_number (0, 1000)) / 1000) * 0.08 + 0.08;
        flare->stretch[index] = (((float) (ply_get_random_number (0, 1000)) / 1000) * 0.1 + 0.3) * flare->y_size[index];
        flare->z_offset_strength[index] = 0.1;
}

static void
flare_update (sprite_t *sprite,
              double    time)
{
        int width;
        int height;
        sprite_data_t* sprite_data = sprite->data;
        flare_t *flare = sprite_data->dat;
        ply_image_t *old_image;
        ply_image_t *new_image;
        uint32_t *old_image_data;
        uint32_t *new_image_data;

        flare->frame_count++;
        if (flare->frame_count % (FRAMES_PER_SECOND / FLARE_FRAMES_PER_SECOND))
                return;

        old_image = flare->image_a;
        new_image = flare->image_b;

        old_image_data = ply_image_get_data (old_image);
        new_image_data = ply_image_get_data (new_image);

        width = ply_image_get_width (new_image);
        height = ply_image_get_height (new_image);


        int b;

        for (b = 0; b < FLARE_COUNT; b++) {
                int flare_line;
                flare->stretch[b] += (flare->stretch[b] * flare->increase_speed[b]) * (1 - (1 / (3.01 - flare->stretch[b])));
                flare->increase_speed[b] -= 0.003;
                flare->z_offset_strength[b] += 0.01;

                if (flare->stretch[b] > 2 || flare->stretch[b] < 0.2)
                        flare_reset (flare, b);
                for (flare_line = 0; flare_line < FLARE_LINE_COUNT; flare_line++) {
                        double x, y, z;
                        double angle, distance;
                        float theta;
                        for (theta = -M_PI + (0.05 * cos (flare->increase_speed[b] * 1000 + flare_line)); theta < M_PI; theta += 0.05) {
                                int ix;
                                int iy;

                                x = (cos (theta) + 0.5) * flare->stretch[b] * 0.8;
                                y = (sin (theta)) * flare->y_size[b];
                                z = x * (sin (b + flare_line * flare_line)) * flare->z_offset_strength[b];

                                float strength = 1.1 - (x / 2) + flare->increase_speed[b] * 3;
                                x += 4.5;
                                if ((x * x + y * y + z * z) < 25) continue;

                                strength = CLAMP (strength, 0, 1);
                                strength *= 32;

                                x += 0.05 * sin (4 * theta * (sin (b + flare_line * 5)));
                                y += 0.05 * cos (4 * theta * (sin (b + flare_line * 5)));
                                z += 0.05 * sin (4 * theta * (sin (b + flare_line * 5)));

                                distance = sqrt (x * x + y * y);
                                angle = atan2 (y, x) + flare->rotate_xy[b] + 0.02 * sin (b * flare_line);
                                x = distance * cos (angle);
                                y = distance * sin (angle);

                                distance = sqrt (z * z + y * y);
                                angle = atan2 (y, z) + flare->rotate_yz[b] + 0.02 * sin (3 * b * flare_line);
                                z = distance * cos (angle);
                                y = distance * sin (angle);

                                distance = sqrt (x * x + z * z);
                                angle = atan2 (z, x) + flare->rotate_xz[b] + 0.02 * sin (8 * b * flare_line);
                                x = distance * cos (angle);
                                z = distance * sin (angle);


                                x *= 41;
                                y *= 41;

                                x += 720 - 800 + width;
                                y += 300 - 480 + height;

                                ix = x;
                                iy = y;
                                if (ix >= (width - 1) || iy >= (height - 1) || ix <= 0 || iy <= 0) continue;

                                uint32_t colour = MIN (strength + (old_image_data[ix + iy * width] >> 24), 255);
                                colour <<= 24;
                                old_image_data[ix + iy * width] = colour;
                        }
                }
        }

        {
                int x, y;
                for (y = 1; y < (height - 1); y++) {
                        for (x = 1; x < (width - 1); x++) {
                                uint32_t value = 0;
                                value += (old_image_data[(x - 1) + (y - 1) * width] >> 24) * 1;
                                value += (old_image_data[x + (y - 1) * width] >> 24) * 2;
                                value += (old_image_data[(x + 1) + (y - 1) * width] >> 24) * 1;
                                value += (old_image_data[(x - 1) + y * width] >> 24) * 2;
                                value += (old_image_data[x + y * width] >> 24) * 8;
                                value += (old_image_data[(x + 1) + y * width] >> 24) * 2;
                                value += (old_image_data[(x - 1) + (y + 1) * width] >> 24) * 1;
                                value += (old_image_data[x + (y + 1) * width] >> 24) * 2;
                                value += (old_image_data[(x + 1) + (y + 1) * width] >> 24) * 1;
                                value /= 21;
                                value = (value << 24) | ((int) (value * 0.7) << 16) | (value << 8) | (value << 0);
                                new_image_data[x + y * width] = value;
                        }
                }
        }
        flare->image_a = new_image;
        flare->image_b = old_image;
        sprite->image = new_image;
        sprite->refresh_me = 1;
        return;
}


static void
sprite_move (view_t   *view,
             sprite_t *sprite,
             double    time)
{
        //TODO: Hopefully temporary, library should take care of this
        sprite->oldx = sprite->x;
        sprite->oldy = sprite->y;
        sprite->oldz = sprite->z;
        sprite_data_t* sprite_data = sprite->data;
        switch (sprite_data->type) {
        case SPRITE_TYPE_STATIC:
                break;
        case SPRITE_TYPE_PROGRESS:
                progress_update (view, sprite, time);
                break;
        case SPRITE_TYPE_FLARE:
                flare_update (sprite, time);
                break;
        case SPRITE_TYPE_SATELLITE:
                satellite_move (view, sprite, time);
                break;
        case SPRITE_TYPE_STAR_BG:
                star_bg_update (view, sprite, time);
                break;
        }
}



static void
loc_view_animate_attime (view_t *view,
                     double  time)
{
        ply_list_node_t *node;
        ply_boot_splash_plugin_t *plugin;

        plugin = view->plugin;

        if (plugin->progress_target >= 0)
                plugin->progress = (plugin->progress * 10 + plugin->progress_target) / 11;

        node = ply_list_get_first_node (view->sprites);
        while (node) {
                sprite_t *sprite = ply_list_node_get_data (node);
                sprite_move (view, sprite, time);
                node = ply_list_get_next_node (view->sprites, node);
        }

        sprite_list_sort (view);

        for (node = ply_list_get_first_node (view->sprites); node; node = ply_list_get_next_node (view->sprites, node)) {
                sprite_t *sprite = ply_list_node_get_data (node);
                if (sprite->x != sprite->oldx ||
                    sprite->y != sprite->oldy ||
                    sprite->z != sprite->oldz ||
                    sprite->refresh_me) {
                        sprite->refresh_me = 0;

                        int width = ply_image_get_width (sprite->image);
                        int height = ply_image_get_height (sprite->image);

                        sprite_data_t* sprite_data = sprite->data;
                        if (sprite_data->type == SPRITE_TYPE_STAR_BG) {
                                star_bg_t *star_bg = sprite_data->dat;
                                int i;
                                for (i = 0; i < star_bg->star_count; i++) {
                                        if (star_bg->star_refresh[i]) {
                                                ply_pixel_display_draw_area (view->display,
                                                                             sprite->x + star_bg->star_x[i], sprite->y + star_bg->star_y[i], 1, 1);
                                                star_bg->star_refresh[i] = 0;
                                        }
                                }
                                continue;
                        }



                        int x = sprite->x - sprite->oldx;
                        int y = sprite->y - sprite->oldy;

                        if (x < width && x > -width && y < height && y > -height) {
                                x = MIN (sprite->x, sprite->oldx);
                                y = MIN (sprite->y, sprite->oldy);
                                width = (MAX (sprite->x, sprite->oldx) - x) + ply_image_get_width (sprite->image);
                                height = (MAX (sprite->y, sprite->oldy) - y) + ply_image_get_height (sprite->image);
                                ply_pixel_display_draw_area (view->display,
                                                             x, y, width, height);
                        } else {
                                ply_pixel_display_draw_area (view->display,
                                                             sprite->x, sprite->y, width, height);
                                ply_pixel_display_draw_area (view->display,
                                                             sprite->oldx, sprite->oldy, width, height);
                        }
                }
        }
}




static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
        
#ifdef  SHOW_LOGO_HALO
        plugin_data_t* plugin_data = plugin->plugin_data;
        ply_image_free (plugin_data->highlight_logo_image);
#endif
}


//TODO: Rewrite background to be a sprite instead


static void
loc_view_setup_scene (view_t *view)
{
        ply_boot_splash_plugin_t *plugin;
        plugin_data_t* plugin_data;
        sprite_t *sprite;
        int i;
        int x, y;
        int width = 360;
        int height = 460;
        unsigned long screen_width, screen_height;
        sprite_data_t* sprite_data; 

        plugin = view->plugin;
        plugin_data = plugin->plugin_data;
        view_data_t* view_data = view->data;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        {
                star_bg_t *star_bg;
                if (view_data->scaled_background_image)
                        ply_image_free (view_data->scaled_background_image);
                view_data->scaled_background_image = ply_image_resize (plugin_data->logo_image, screen_width, screen_height);
                star_bg = malloc (sizeof(star_bg_t));
                star_bg->star_count = (screen_width * screen_height) / 400;
                star_bg->star_x = malloc (sizeof(int) * star_bg->star_count);
                star_bg->star_y = malloc (sizeof(int) * star_bg->star_count);
                star_bg->star_refresh = malloc (sizeof(int) * star_bg->star_count);
                star_bg->frame_count = 0;
                sprite_data = malloc(sizeof(sprite_data_t));
                sprite_data->dat = star_bg;
                sprite_data->type = SPRITE_TYPE_STAR_BG;
                
                sprite = add_sprite (view, view_data->scaled_background_image, star_bg);
                sprite->z = -10000;

                uint32_t *image_data = ply_image_get_data (view_data->scaled_background_image);
                for (y = 0; y < (int) screen_height; y++) {
                        for (x = 0; x < (int) screen_width; x++) {
                                image_data[x + y * screen_width] = star_bg_gradient_colour (x, y, screen_width, screen_height, false, 0);
                        }
                }

                for (i = 0; i < star_bg->star_count; i++) {
                        do {
                                x = ply_get_random_number (0, screen_width);
                                y = ply_get_random_number (0, screen_height);
                        } while (image_data[x + y * screen_width] == 0xFFFFFFFF);
                        star_bg->star_refresh[i] = 0;
                        star_bg->star_x[i] = x;
                        star_bg->star_y[i] = y;
                        image_data[x + y * screen_width] = 0xFFFFFFFF;
                }
                for (i = 0; i < (int) (screen_width * screen_height) / 400; i++) {
                        x = ply_get_random_number (0, screen_width);
                        y = ply_get_random_number (0, screen_height);
                        image_data[x + y * screen_width] = star_bg_gradient_colour (x, y, screen_width, screen_height, true, ((float) x * y * 13 / 10000));
                }

                for (i = 0; i < star_bg->star_count; i++) {
                        image_data[star_bg->star_x[i] + star_bg->star_y[i] * screen_width] =
                                star_bg_gradient_colour (star_bg->star_x[i], star_bg->star_y[i], screen_width, screen_height, true, 0.0);
                }
        }
        
        sprite_data = malloc(sizeof(sprite_data_t));
        sprite_data->dat = NULL;
        sprite_data->type = SPRITE_TYPE_STATIC;
        
        sprite = add_sprite (view, plugin_data->logo_image, sprite_data);
        sprite->x = screen_width / 2 - ply_image_get_width (plugin_data->logo_image) / 2;
        sprite->y = screen_height / 2 - ply_image_get_height (plugin_data->logo_image) / 2;
        sprite->z = 1000;

#ifdef SHOW_LOGO_HALO
        plugin_data->highlight_logo_image = ply_image_resize (plugin_data->logo_image, ply_image_get_width (plugin_data->logo_image) + HALO_BLUR * 2, ply_image_get_height (plugin_data->logo_image) + HALO_BLUR * 2);
        highlight_image (plugin_data->highlight_logo_image, plugin_data->logo_image, HALO_BLUR);
        
        
        sprite_data = malloc(sizeof(sprite_data_t));
        sprite_data->dat = NULL;
        sprite_data->type = SPRITE_TYPE_STATIC;
        
        sprite = add_sprite (view, plugin_data->highlight_logo_image, sprite_data);
        sprite->x = 10 - HALO_BLUR;
        sprite->y = 10 - HALO_BLUR;
        sprite->z = -910;
#endif

        
        sprite_data = malloc(sizeof(sprite_data_t));
        sprite_data->dat = NULL;
        sprite_data->type = SPRITE_TYPE_STATIC;
        
        sprite = add_sprite (view, plugin_data->star_image, sprite_data);
        sprite->x = screen_width - ply_image_get_width (plugin_data->star_image);
        sprite->y = screen_height - ply_image_get_height (plugin_data->star_image);
        sprite->z = 0;
#ifdef  SHOW_PLANETS
        for (i = 0; i < 5; i++) {
                satellite_t *satellite = malloc (sizeof(satellite_t));
                satellite->type = SATELLITE_TYPE_PLANET;
                satellite->end_x = satellite->start_x = 720 - 800 + screen_width;
                satellite->end_y = satellite->start_y = 300 - 480 + screen_height;

                satellite->distance = i * 100 + 280;
                satellite->theta = M_PI * 0.8;
                satellite->image = plugin_data->planet_image[i];
                satellite->image_altered = ply_image_resize (satellite->image, ply_image_get_width (satellite->image), ply_image_get_height (satellite->image));
                
                
                sprite_data = malloc(sizeof(sprite_data_t));
                sprite_data->dat = satellite;
                sprite_data->type = SPRITE_TYPE_SATELLITE;
                
                
                sprite = add_sprite (view, satellite->image_altered, sprite_data);
                satellite_move (plugin, sprite, 0);
        }
#endif
#ifdef  SHOW_COMETS
        for (i = 0; i < 1; i++) {
                satellite_t *satellite = malloc (sizeof(satellite_t));
                satellite->type = SATELLITE_TYPE_COMET;
                satellite->end_x = satellite->start_x = 720 - 800 + screen_width;
                satellite->end_y = satellite->start_y = 300 - 480 + screen_height;
                satellite->distance = 550 + i * 50;
                satellite->theta = M_PI * 0.8;
#define COMET_SIZE 64
                satellite->image = ply_image_resize (plugin_data->progress_barimage, COMET_SIZE, COMET_SIZE);
                satellite->image_altered = ply_image_resize (satellite->image, COMET_SIZE, COMET_SIZE);
                uint32_t *image_data = ply_image_get_data (satellite->image);
                uint32_t *image_altered_data = ply_image_get_data (satellite->image_altered);


                for (y = 0; y < COMET_SIZE; y++) {
                        for (x = 0; x < COMET_SIZE; x++) {
                                image_data[x + y * COMET_SIZE] = 0x0;
                                image_altered_data[x + y * COMET_SIZE] = 0x0;
                        }
                }

                
                sprite_data = malloc(sizeof(sprite_data_t));
                sprite_data->dat = satellite;
                sprite_data->type = SPRITE_TYPE_SATELLITE;
                
                sprite = add_sprite (view, satellite->image_altered, plugin_data);
                for (x = 0; x < COMET_SIZE; x++) {
                        satellite_move (plugin, sprite, 0);
                }
        }
#endif

#ifdef  SHOW_PROGRESS_BAR
        progress_t *progress = malloc (sizeof(progress_t));

        progress->image = plugin_data->progress_barimage;

        x = screen_width / 2 - ply_image_get_width (plugin_data->logo_image) / 2;
        y = screen_height / 2 + ply_image_get_height (plugin_data->logo_image) / 2 + 20;
        progress->image_altered = ply_image_resize (plugin_data->progress_barimage, ply_image_get_width (plugin_data->logo_image), ply_image_get_height (plugin_data->progress_barimage));
        progress->start_width = 1;
        progress->end_width = ply_image_get_width (plugin_data->logo_image);
        progress->current_width = 0;

        
        sprite_data = malloc(sizeof(sprite_data_t));
        sprite_data->dat = progress;
        sprite_data->type = SPRITE_TYPE_PROGRESS;
        
        sprite = add_sprite (view, progress->image_altered, sprite_data);
        sprite->x = x;
        sprite->y = y;
        sprite->z = 10011;
        progress_update (view, sprite, 0);



#endif

        flare_t *flare = malloc (sizeof(flare_t));

        flare->image_a = ply_image_resize (plugin_data->star_image, width, height);
        flare->image_b = ply_image_resize (plugin_data->star_image, width, height);

        
        sprite_data = malloc(sizeof(sprite_data_t));
        sprite_data->dat = flare;
        sprite_data->type = SPRITE_TYPE_FLARE;
        
        sprite = add_sprite (view, flare->image_a, sprite_data);
        sprite->x = screen_width - width;
        sprite->y = screen_height - height;
        sprite->z = 1;

        sprite_list_sort (view);

        uint32_t *old_image_data = ply_image_get_data (flare->image_a);
        uint32_t *new_image_data = ply_image_get_data (flare->image_b);


        for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                        new_image_data[x + y * width] = 0x0;
                        old_image_data[x + y * width] = 0x0;
                }
        }

        for (i = 0; i < FLARE_COUNT; i++) {
                flare_reset (flare, i);
        }
        flare->frame_count = 0;
        flare_update (sprite, i);
}



static bool
loc_show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_boot_splash_mode_t    mode)
{
        assert (plugin != NULL);
        
        plugin_data_t* plugin_data;
        plugin_data = plugin->plugin_data;
        
        assert (plugin_data->logo_image != NULL);
        
        ply_trace ("loading logo image");
        if (!ply_image_load (plugin_data->logo_image))
                return false;

        ply_trace ("loading star image");
        if (!ply_image_load (plugin_data->star_image))
                return false;

        ply_trace ("loading planet images");
#ifdef  SHOW_PLANETS
        if (!ply_image_load (plugin_data->planet_image[0]))
                return false;
        if (!ply_image_load (plugin_data->planet_image[1]))
                return false;
        if (!ply_image_load (plugin_data->planet_image[2]))
                return false;
        if (!ply_image_load (plugin_data->planet_image[3]))
                return false;
        if (!ply_image_load (plugin_data->planet_image[4]))
                return false;
#endif
#ifdef  SHOW_PROGRESS_BAR
        if (!ply_image_load (plugin_data->progress_barimage))
                return false;
#endif

        ply_trace ("loading lock image");
        if (!ply_image_load (plugin_data->lock_image))
                return false;

        ply_trace ("loading box image");
        if (!ply_image_load (plugin_data->box_image))
                return false;

        return true;
}






//void loc_add_pixel_display(ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display, view_t *view)
//{
//        plugin_data_t *plugin_dat = plugin->plugin_data;
//        ply_trace("display added: %p, %p", display, view);
//        sprite_t *back = add_sprite(view, ply_image_resize(plugin_dat->back, ply_pixel_display_get_width (display), ply_pixel_display_get_height (display)), NULL);
//        back->x = 0;
//       back->y = 0;
//       back->z = -1;
        
//}

plugin_base_interface_t* create_base_interface()
{
        ply_trace ("Plugin Base interface created\n\n");
        static plugin_base_interface_t plugin_interface =
        {
                .create_plugin_data        = create_plugin_data,
                .view_setup_scene        = loc_view_setup_scene,
                .view_animate_attime        = loc_view_animate_attime,
                //.add_pixel_display          = loc_add_pixel_display
                .view_create_data        =view_create_data,
                .view_free_data         = view_free_data,
                .view_load              =loc_view_load,
                .view_show_prompt       =loc_view_show_prompt,
                .view_hide_prompt       = loc_view_hide_prompt,
                .view_show_message      = loc_view_show_message,
                .destroy_plugin_data    = destroy_plugin_data,
                .free_sprite_data       = free_sprite_data,
                .stop_animation         = stop_animation,
                .show_splash_screen     = loc_show_splash_screen,
                
        };

        return &plugin_interface;
}
