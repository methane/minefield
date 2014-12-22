from setuptools import Extension, setup

import os
import sys
import os.path
import platform
import fnmatch

develop = False
if os.environ.get("MINEFIELD_DEVELOP") == "1":
    develop = True
# develop = True 


def read(name):
    return open(os.path.join(os.path.dirname(__file__), name)).read()

def check_platform():
    if "posix" not in os.name:
        print("Are you really running a posix compliant OS ?")
        print("Be posix compliant is mandatory")
        sys.exit(1)


def get_picoev_file():
    poller_file = None

    if "Linux" == platform.system():
        poller_file = 'minefield/server/picoev_epoll.c'
    elif platform.system() in ('Darwin', 'FreeBSD'):
        poller_file = 'minefield/server/picoev_kqueue.c'
    else:
        print("Sorry, not support .")
        sys.exit(1)
    return poller_file

def get_sources(path, ignore_files):
    src = []
    for root, dirs , files in os.walk(path):
        for file in files:
            src_path = os.path.join(root, file)
            #ignore = reduce(lambda x, y: x or y, [fnmatch.fnmatch(src_path, i) for i in ignore_files])
            ignore = [i for i in ignore_files if  fnmatch.fnmatch(src_path, i)]
            if not ignore and src_path.endswith(".c"):
                src.append(src_path)
    return src

check_platform()

define_macros=[
        ("HTTP_PARSER_DEBUG", "0") ]
extra_compile_args = []
install_requires=[]

if develop:
    define_macros.append(("DEVELOP", None))
    extra_compile_args.append('-Og')

sources = get_sources("minefield", ["*picoev_*"])
sources = """
minefield/server/buffer.c
minefield/server/client.c
minefield/server/heapq.c
minefield/server/http_request_parser.c
minefield/server/input.c
minefield/server/log.c
minefield/server/request.c
minefield/server/response.c
minefield/server/server.c
minefield/server/time_cache.c
minefield/server/timer.c
minefield/server/util.c
minefield/server/picohttpparser/picohttpparser.c
""".split()
sources.append(get_picoev_file())


setup(name='minefield',
    version="0.6",
    description="High performance Python WSGI Web Server",
    long_description=read('README.rst'),
    author='INADA Naoki',
    author_email='songofacandy@gmail.com',
    license='BSD',
    platforms='Linux, BSD, Darwin',
    packages= ['minefield'],
    install_requires=install_requires,

    entry_points="""
    [gunicorn.workers]
    gunicorn_worker=minefield.gminefield:MinefieldWorker
    """,
    ext_modules = [
        Extension('minefield.server',
            sources=sources,
            include_dirs=[],
            library_dirs=[],
            define_macros=define_macros,
            extra_compile_args=extra_compile_args,
        )],

    classifiers=[
        'Development Status :: 4 - Beta',
        'Environment :: Web Environment',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: POSIX :: Linux',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: POSIX :: BSD :: FreeBSD',
        'Programming Language :: C',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'
    ],
)

