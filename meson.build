plugin_name = 'template-flares'
template_plugin = shared_module(plugin_name,
  'plugin.c', 'plugin_base.c',
  dependencies: [
    libply_splash_core_dep,
    libply_splash_graphics_dep,
  ],
  c_args: [
    '-DPLYMOUTH_LOGO_FILE="@0@"'.format(plymouth_logo_file),
    '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    '-DFRAMES_PER_SECOND=60',
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path,
)
