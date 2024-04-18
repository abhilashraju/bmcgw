
rproxy_includes = include_directories(['./src'])
rproxy_sources = [
    'src/rproxy.cpp'
]


bmcgw = executable('rproxy', 
    [rproxy_sources], 
    include_directories : [core_includes,rproxy_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )