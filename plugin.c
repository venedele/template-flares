#include "plugin_base.h"
#include "math.h"

#define PI 3.14159265358979323846

struct _plugin_data {
        ply_image_t                   *img;
        ply_image_t                   *back;
        double			      nultime;
};

plugin_data_t* create_plugin_data(ply_boot_splash_plugin_t * plugin, ply_key_file_t* key_file)
{
        plugin_data_t* plugin_data = calloc (1, sizeof(plugin_data_t));
        
        char *image_dir, *image_path;
        image_dir = plugin->image_dir;//= ply_key_file_get_value (key_file, PLUGIN_NAME, "ImageDir");

        asprintf (&image_path, "%s/lock.png", image_dir);
        plugin_data->img = ply_image_new (image_path);
        ply_image_load(plugin_data->img);
        free (image_path);
        
        
        asprintf (&image_path, "%s/entry1.png", image_dir);
        plugin_data->back = ply_image_new (image_path);
        
        ply_image_load(plugin_data->back);
        free (image_path);
        
        plugin_data->nultime = 0;
        return plugin_data;
}

void loc_view_setup_scene(view_t* view)
{
        plugin_data_t *plugin = view->plugin->plugin_data;
        sprite_t *sprite;
        ply_trace("Setting up scene");
        sprite = add_sprite(view, plugin->img, NULL);
        sprite->x = 500;
        sprite->y = 500;
        sprite->data = (void*)500;
	plugin->nultime = ply_get_timestamp();
}

void loc_view_animate_attime(view_t *view, double time)
{
	//ply_trace("Time %.6f: %.6f", time, t_sec);
        
    ply_list_node_t *node;
    node = ply_list_get_first_node (view->sprites);
    sprite_t *sprite;
    while (node!=NULL) {
        sprite = ply_list_node_get_data(node);
        if(sprite->data == (void*)500) break;
        node = ply_list_get_next_node (view->sprites, node);
    }

        plugin_data_t *plugin = view->plugin->plugin_data;
        double t_sec = time-plugin->nultime;
        //(x-h)+(y-k)=100^2
        double ang_speed = 0.5;
        //x = r*cos(a)
        //y = r*sin(a)
        double alpha = t_sec*ang_speed;
	alpha = alpha - floor(alpha/(2*PI))*2*PI;
	sprite->oldx = sprite->x;
	sprite->oldy = sprite->y;
	sprite->x = 500 + 100 - (int)(100*cos(alpha));
	sprite->y = 500 - (int)(100*sin(alpha));
	sprite->refresh_me = 1;

}

void loc_add_pixel_display(ply_boot_splash_plugin_t *plugin, ply_pixel_display_t *display, view_t *view)
{
        plugin_data_t *plugin_dat = plugin->plugin_data;
        ply_trace("display added: %p, %p", display, view);
        sprite_t *back = add_sprite(view, ply_image_resize(plugin_dat->back, ply_pixel_display_get_width (display), ply_pixel_display_get_height (display)), NULL);
        back->x = 0;
        back->y = 0;
        back->z = -1;
        
}

plugin_base_interface_t* create_base_interface()
{
        ply_trace ("Plugin Base interface created\n\n");
        static plugin_base_interface_t plugin_interface =
        {
                .create_plugin_data        = create_plugin_data,
                .view_setup_scene        = loc_view_setup_scene,
                .view_animate_attime        = loc_view_animate_attime,
                .add_pixel_display          = loc_add_pixel_display
        };

        return &plugin_interface;
}
