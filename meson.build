# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019-2022 Intel Corporation

project('CNDP', 'C', 'cpp',
    # Get version number from file.
    # Fallback to "more" for Windows compatibility.
    version: run_command(find_program('cat', 'more'),
        files('VERSION')).stdout().strip(),
    license: 'BSD',
    default_options: [
        'buildtype=release',
        'default_library=shared',
        'warning_level=3',
        'werror=true'
    ],
    meson_version: '>= 0.53.2'
)

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

cndp_source_root = meson.current_source_dir()
cndp_build_root = meson.current_build_dir()
enable_asserts = get_option('enable_asserts')
cne_verbose = get_option('verbose')
cndp_libs = []
extra_ldflags = []
enabled_libs = []
cndp_pmds = []

cne_conf = configuration_data()

machine_args = []

##################################################################
# set the major version, which might be used by pmds and libraries
# depending on the configuration options
pver = meson.project_version().split('.')
major_version = '@0@.@1@'.format(pver.get(0), pver.get(1))

# extract all version information into the build configuration
cne_conf.set('CNE_VER_YEAR', pver.get(0).to_int())
cne_conf.set('CNE_VER_MONTH', pver.get(1).to_int())
if pver.get(2).contains('-rc')
    rc_ver = pver.get(2).split('-rc')
    cne_conf.set('CNE_VER_MINOR', rc_ver.get(0).to_int())
    cne_conf.set_quoted('CNE_VER_SUFFIX', '-rc')
    cne_conf.set('CNE_VER_RELEASE', rc_ver.get(1).to_int())
else
    cne_conf.set('CNE_VER_MINOR', pver.get(2).to_int())
    cne_conf.set_quoted('CNE_VER_SUFFIX', '')
# for actual, non-rc releases, set the release value to 99 to ensure releases
# have higher version numbers than their respective release candidates
    cne_conf.set('CNE_VER_RELEASE', 99)
endif

cne_conf.set10('CNE_ENABLE_ASSERT', enable_asserts)

machine = get_option('machine')

# machine type 'default' is special, it defaults to the per arch agreed common
# minimal baseline needed for CNDP.
# That might not be the most optimized, but the most portable version while
# still being able to support the CPU features required for CNDP.
# This can be bumped up by the CNDP project, but it can never be an
# invariant like 'native'
if machine == 'default'
    if host_machine.cpu_family().startswith('x86')
        # matches the old pre-meson build systems default
        machine = 'corei7'
    endif
endif

cne_conf.set('CNE_MACHINE', machine)
cne_conf.set('CNE_CACHE_LINE_SIZE', 64)

machine_args += '-march=' + machine

toolchain = cc.get_id()
cne_conf.set_quoted('CNE_TOOLCHAIN', toolchain)
cne_conf.set('CNE_TOOLCHAIN_' + toolchain.to_upper(), 1)

cne_conf.set('CNE_ARCH_64', cc.sizeof('void *') == 8)

add_project_link_arguments('-Wl,--no-as-needed', language: 'c')
if get_option('buildtype') == 'release'
    add_project_arguments('-g', language: 'c')
endif

# use pthreads
add_project_link_arguments('-pthread', language: 'c')
extra_ldflags += '-pthread'

# on Alpine, execinfo and backtrace are in a separate library
if not cc.has_header('execinfo.h')
    error('Missing execinfo.h. Try installing "libexecinfo-dev".')
endif

if cc.find_library('libexecinfo', required : false).found()
    add_project_link_arguments('-lexecinfo', language: 'c')
    extra_ldflags += '-lexecinfo'
endif

# on some OS, maths functions are in a separate library
if cc.find_library('libm', required : false).found()
    # some libs depend on maths lib
    add_project_link_arguments('-lm', language: 'c')
    extra_ldflags += '-lm'
endif

# if link_lib is empty, do not add it to project properties
add_project_link_arguments('-ldl', language: 'c')
extra_ldflags += '-ldl'

# check for libraries used in multiple places in CNDP
has_libnuma = 0
numa_dep = cc.find_library('numa', required: false)
if numa_dep.found() and cc.has_header('numaif.h')
    cne_conf.set10('CNE_HAS_LIBNUMA', true)
    has_libnuma = 1
    add_project_link_arguments('-lnuma', language: 'c')
    extra_ldflags += '-lnuma'
endif

# check for libbsd
libbsd = dependency('libbsd', required: true)
if libbsd.found()
    add_project_link_arguments('-lbsd', language: 'c')
    cne_conf.set('CNE_USE_LIBBSD', 1)
endif

# check for libjson-c
json_c = dependency('json-c', required: true)
if json_c.found()
    add_project_link_arguments('-ljson-c', language: 'c')
    cne_conf.set('CNE_USE_JSON_C', 1)
    extra_ldflags += '-ljson-c'
endif

# Check for libpcap for txgen
if cc.find_library('libpcap', required : true).found()
    add_project_link_arguments('-lpcap', language: 'c')
    extra_ldflags += '-lpcap'
endif

add_project_arguments('-I/usr/include/libnl3', language: 'c')

nl_dep = dependency('libnl-3.0', required: true, method: 'pkg-config')
if nl_dep.found()
    add_project_link_arguments('-lnl-3', language: 'c')
    extra_ldflags += '-lnl-3'
endif

nl_cli_dep = dependency('libnl-cli-3.0', required: true, method: 'pkg-config')
if nl_cli_dep.found()
    add_project_link_arguments('-lnl-cli-3', language: 'c')
    extra_ldflags += '-lnl-cli-3'
endif

nl_route_dep = dependency('libnl-route-3.0', required: true, method: 'pkg-config')
if nl_cli_dep.found()
    add_project_link_arguments('-lnl-route-3', language: 'c')
    extra_ldflags += '-lnl-route-3'
endif

cne_conf.set10('HAS_XSK_UMEM_SHARED', false)
bpf_dep = dependency('libbpf', version: ['>=0.3.0', '<=0.6.1'], required: true, method: 'pkg-config')
if bpf_dep.found() and cc.has_header('bpf/xsk.h') and cc.has_header('linux/if_xdp.h')
    extra_ldflags += '-lbpf'
    cne_conf.set10('HAS_XSK_UMEM_SHARED', true)
endif

cne_conf.set('HAS_UINTR_SUPPORT', false)
if cc.has_argument('-muintr') and cc.get_id() == 'gcc' and cc.has_header('x86gprintrin.h')
    message('GCC Version: ' + cc.version())
    cne_conf.set('HAS_UINTR_SUPPORT', true)
    add_project_arguments('-muintr', language: 'c')
endif

# Check for libdlb
libdlb = cc.find_library('dlb', required: false)
if libdlb.found()
    add_project_arguments('-DDLB2', language: 'c')
    add_project_arguments('-I/usr/local/include/dlb', language: 'c')
    add_project_link_arguments('-ldlb', language: 'c')
    add_project_link_arguments('-lrt', language: 'c')
    extra_ldflags += '-ldlb'
endif


# for clang 32-bit compiles we need libatomic for 64-bit atomic ops
if cc.get_id() == 'clang'
    atomic_dep = cc.find_library('atomic', required: true)
    add_project_link_arguments('-latomic', language: 'c')
    extra_ldflags += '-latomic'
endif

# enable extra warnings and disable any unwanted warnings
warning_flags = [
    # additional warnings in alphabetical order
    '-Wno-pedantic',
    '-Wcast-qual',
    '-Wdeprecated',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wpointer-arith',
    '-Wsign-compare',
    '-Wstrict-prototypes',
    '-Wundef',
    '-Wwrite-strings',

    # globally disabled warnings
    '-Wno-address-of-packed-member',
    '-Wno-packed-not-aligned',
    '-Wno-missing-field-initializers',
]
foreach arg: warning_flags
    if cc.has_argument(arg)
        add_project_arguments(arg, language: 'c')
    endif
    if cpp.has_argument(arg)
        add_project_arguments(arg, language: 'cpp')
    endif
endforeach

compile_time_cpuflags = []

# get binutils version for the workaround of Bug 97
ldver = run_command('ld', '-v').stdout().strip()
if ldver.contains('2.30') and cc.has_argument('-mno-avx512f')
    machine_args += '-mno-avx512f'
    message('Binutils 2.30 detected, disabling AVX512 support as workaround for bug #97')
endif
if ldver.contains('2.31') and cc.has_argument('-mno-avx512f')
    machine_args += '-mno-avx512f'
    message('Binutils 2.31 detected, disabling AVX512 support as workaround for bug #249')
endif

# we require SSE4.2 for CNDP
sse_errormsg = '''SSE4.2 instruction set is required for CNDP.
Please set the machine type to "nehalem" or "corei7" or higher value'''

if cc.get_define('__SSE4_2__', args: machine_args) == ''
    error(sse_errormsg)
endif

base_flags = ['SSE', 'SSE2', 'SSE3','SSSE3', 'SSE4_1', 'SSE4_2']
foreach f:base_flags
    cne_conf.set('CNE_MACHINE_CPUFLAG_' + f, 1)
    compile_time_cpuflags += ['CNE_CPUFLAG_' + f]
endforeach

optional_flags = ['AES', 'PCLMUL',
        'AVX', 'AVX2', 'AVX512F',
        'RDRND', 'RDSEED']
foreach f:optional_flags
    if cc.get_define('__@0@__'.format(f), args: machine_args) == '1'
        if f == 'PCLMUL' # special case flags with different defines
            f = 'PCLMULQDQ'
        elif f == 'RDRND'
            f = 'RDRAND'
        endif
        cne_conf.set('CNE_MACHINE_CPUFLAG_' + f, 1)
        compile_time_cpuflags += ['CNE_CPUFLAG_' + f]
    endif
endforeach

cne_conf.set('CNE_ARCH_X86', 1)
if cne_conf.get('CNE_ARCH_64')
    cne_conf.set('CNE_ARCH_X86_64', 1)
    cne_conf.set('CNE_ARCH', 'x86_64')
else
    cne_conf.set('CNE_ARCH_I686', 1)
    cne_conf.set('CNE_ARCH', 'i686')
endif

cne_conf.set('CNE_COMPILE_TIME_CPUFLAGS', ','.join(compile_time_cpuflags))

# specify -D_GNU_SOURCE unconditionally
add_project_arguments('-D_GNU_SOURCE', language: 'c')

# configure the build, and make sure configs here and in config folder are
# able to be included in any file. We also store a global array of include dirs
# for passing to pmdinfogen scripts
global_inc = include_directories('.')
build_cfg_file = 'cne_build_config.h'
add_project_arguments(['-include', build_cfg_file] + machine_args, language: 'c')
add_project_arguments(['-include', build_cfg_file] + machine_args, language: 'cpp')

if get_option('coverity')
    message('*** Building for Coverity ***')
    add_project_arguments(['-include', '/opt/config/fix-coverity.h'], language: 'c')
endif

cne_conf.set10('COVERITY_BUILD', get_option('coverity'))

# compile AVX2 version if either:
# a. we have AVX supported in minimum instruction set baseline
# b. it's not minimum instruction set, but supported by compiler
#
# in former case, just add avx2 C file to files list
# in latter case, compile c file to static lib, using correct compiler
# flags, and then have the .o file from static lib linked into main lib.
if cne_conf.has('CNE_MACHINE_CPUFLAG_AVX2')
    add_project_arguments('-DCC_AVX2_SUPPORT', language: 'c')
elif cc.has_argument('-mavx2')
    add_project_arguments('-DCC_AVX2_SUPPORT', language: 'c')
endif

# compile AVX512 version if either:
# a. we have AVX512F supported in minimum instruction set baseline
# b. it's not minimum instruction set, but supported by compiler
#
# in former case, just add avx512 C file to files list
# in latter case, compile c file to static lib, using correct
# compiler flags, and then have the .o file from static lib
# linked into main lib.

# check if all required flags already enabled (variant a).
avx512_flags = ['__AVX512F__','__AVX512DQ__']
avx512_on = true
foreach f:avx512_flags
    if cc.get_define(f, args: machine_args) == ''
        avx512_on = false
    endif
endforeach

cne_conf.set10('CNET_ENABLE_TCP', get_option('enable_tcp'))
if get_option('enable_tcp')
    message('*** TCP support enabled')
else
    message('*** TCP support disabled')
endif

# write the build config into the cndp include directory
configure_file(output: build_cfg_file,
    configuration: cne_conf,
    install_dir: join_paths(get_option('includedir'), meson.project_name().to_lower()))

build_cfg = declare_dependency(include_directories: include_directories('.'))

##################################################################

subdir('lib')               # Build Libs
subdir('doc')               # Build Docs
subdir('test')              # Build Tests
subdir('examples')          # Build Examples
subdir('usrtools')          # Build Usertools

cndp_pmds = ['-Wl,--whole-archive'] + cndp_pmds + ['-Wl,--no-whole-archive']

libcndp_a = []
libcndp_so = []
foreach lib:enabled_libs
    libcndp_a += 'lib' + lib + '.a '
    libcndp_so += 'lib' + lib + '.so '
endforeach

cndp_a_name = 'libcndp.a'
cndp_so_name = 'libcndp.so'

mklib = find_program('tools/mklib.sh')
run_command(mklib, cndp_a_name, libcndp_a)
run_command(mklib, cndp_so_name, libcndp_so)

cndp_a = custom_target('libcndp_a_target',
    output: cndp_a_name,
    input: join_paths('/tmp', cndp_a_name),
    command:[find_program('cp'), '@INPUT@', cndp_a_name],
    install_dir: join_paths('lib', 'x86_64-linux-gnu'),
    install: true)
cndp_so = custom_target('libcndp_so_target',
    output: cndp_so_name,
    input: join_paths('/tmp', cndp_so_name),
    command:[find_program('cp'), '@INPUT@', cndp_so_name],
    install_dir: join_paths('lib', 'x86_64-linux-gnu'),
    install: true)

pkg = import('pkgconfig')

message('>>> Create pkg-config file')
pkg.generate(name: meson.project_name(),
    filebase: 'lib' + meson.project_name().to_lower(),
    version: meson.project_version(),
    libraries: ['-Wl,--as-needed'] + cndp_libs,
    libraries_private: cndp_pmds + ['-lcndp'] + ['-Wl,-Bdynamic'] + extra_ldflags,
    requires: libbsd, # if libbsd is not enabled, then this is blank
    description: '''The Cloud Native Data Plane (CNDP).
Note that CFLAGS might contain an -march flag higher than typical baseline.
This is required for a number of static inline functions in the public headers.''',
    subdirs: ['cndp', 'cndp/cnet'],
    extra_cflags: ['-include', build_cfg_file] + machine_args,
    install_dir: 'lib/pkgconfig'
)
message('<<< Done pkg-config file')
