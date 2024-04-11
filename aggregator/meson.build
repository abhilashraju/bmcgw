

aggregator_includes = include_directories(['./src'])
aggregator_sources = [
    'src/aggregator.cpp'
]


aggregator = executable('aggregator', 
    [aggregator_sources], 
    include_directories : [core_includes,aggregator_includes],
    dependencies : [reactor_dep,boost_dep,openssl_dep], 
    )




