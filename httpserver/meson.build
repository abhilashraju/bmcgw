
httpserver_includes = include_directories(['./src'])
httpserver_sources = [
    'src/httpserver.cpp'
]

bmcgw = executable('httpserver', 
    [httpserver_sources], 
    include_directories : [core_includes,httpserver_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )