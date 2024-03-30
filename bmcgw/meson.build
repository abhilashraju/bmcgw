

bmcgw_includes = include_directories(['./src'])
bmcgw_sources = [
    'src/bmcgw.cpp'
]


bmcgw = executable('bmcgw', 
    [bmcgw_sources], 
    include_directories : [core_includes,bmcgw_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )




