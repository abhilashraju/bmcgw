
cert_generator_includes = include_directories(['./src'])
cert_generator_sources = [
    'src/cert_generator.cpp'
]

bmcgw = executable('cert_generator', 
    [cert_generator_sources], 
    include_directories : [core_includes,cert_generator_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )