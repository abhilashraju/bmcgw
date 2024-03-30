
uploader_includes = include_directories(['./src'])
uploader_sources = [
    'src/credcopier.cpp'
]


bmcgw = executable('credcopier', 
    [uploader_sources], 
    include_directories : [core_includes,uploader_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )