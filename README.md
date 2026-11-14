# What is template-flares

`template-flares` is a refactoring and slight upgrade to the `space-flares` plymouth plugin. The end goal of this plugin is to simplify the plymouth splash plugin development by splitting the common code used in all the base plymouth plugins into a separate library. This allows developers to focus on developing nice graphics split into a clean plugin.c file while the house keeping happens in the background.

 *NOTE:* `template-flares` and the `plugin_base` library are currently a quick and dirty coding job, serving as a stepping stone in the proof of concept for a bigger project but might receive some support further in the future.

# What is plugin_base

`plugin_base` is a library containing the plymouth plugin display, view and sprite management code. Including `plugin_base.h` and creating a `create_base_interface function` allows for the creation of callbacks, similar to how ply_boot_splash_plugin_interface works. While the library will implement callbacks to all functions present in the plymouth interface, using only the newly added `view_*` callbacks are enough to create a functioning splash.
