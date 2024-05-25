
httpclient_includes = include_directories(['./src'])
httpclient_sources = [
    'src/httpclient.cpp'
]

bmcgw = executable('httpclient', 
    [httpclient_sources], 
    include_directories : [core_includes,httpclient_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )