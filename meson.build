project('eduardo-c', 'c',
  version : '0.1',
  default_options : ['warning_level=3']
)

# Just support a static library
lib = static_library('eduardo_c', 
  sources : [
    'ecs.c',
    'array_stack.c'
  ],
  install : true,
  gnu_symbol_visibility : 'hidden',
)

# Make this library usable as a Meson subproject.
eduardo_c_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : lib
)
