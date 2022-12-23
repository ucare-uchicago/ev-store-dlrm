from distutils.core import setup
from Cython.Build import cythonize
from distutils.extension import Extension
from Cython.Distutils import build_ext
# extensions = [
#     Extension('EvLFU', ['EvLFU.pyx', 'evlfu_v2.cpp'],
#               extra_compile_args=['-std=c++11'],
#               language='c++'
#               ),
# ]

# setup(
#     ext_modules=cythonize(extensions),
#     # extra_compile_args=["-w", '-g'],
#     # extra_compile_args=["-O3"],
# )

ext_modules = [Extension("EvLFU", ["EvLFU.pyx", "evlfu.cpp"], language='c++',)]

setup(cmdclass = {'build_ext': build_ext}, ext_modules = ext_modules)