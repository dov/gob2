flex = find_program('flex', required: false)

lgen = generator(flex,
                 output : '@PLAINNAME@.yy.c',
                 arguments : ['-o', '@OUTPUT@', '@INPUT@'])

bison = find_program('bison', required: false)
pgen = generator(bison,
                 output : ['@BASENAME@.c', '@BASENAME@.h'],
                 arguments : ['@INPUT@',
                              '--defines=@OUTPUT1@',
                              '--output=@OUTPUT0@'])

pfiles = pgen.process('parse.y')
lfiles = lgen.process('lexer.l')

src = [
'checks.c',
'main.c',
'out.c',
'treefuncs.c',
'util.c',
]

glib_dep = dependency('glib-2.0')

gob2_exe = executable('gob2',
                      src,
                      pfiles,
                      lfiles,
                      include_directories: '..',
                      dependencies: [glib_dep],
                     )
