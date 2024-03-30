
filesync_includes = include_directories(['./src'])
filesync_sources = [
    'src/filesync.cpp'
]


bmcgw = executable('filesync', 
    [filesync_sources], 
    include_directories : [core_includes,filesync_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )