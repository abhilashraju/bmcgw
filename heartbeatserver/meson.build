
heartbeatserver_includes = include_directories(['./src'])
heartbeatserver_sources = [
    'src/heartbeatserver.cpp'
]

bmcgw = executable('heartbeatserver', 
    [heartbeatserver_sources], 
    include_directories : [core_includes,heartbeatserver_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )