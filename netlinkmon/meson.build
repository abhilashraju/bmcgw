netlinkmon_includes = include_directories(['./src'])
netlinkmon_sources = [
    'src/netlinkmon.cpp'
]

bmcgw = executable('netlinkmon', 
    [netlinkmon_sources], 
    include_directories : [core_includes,netlinkmon_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )