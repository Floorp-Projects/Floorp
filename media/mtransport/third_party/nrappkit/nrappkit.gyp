# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# nrappkit.gyp
#
#
{
  'targets' : [
      {
        	'target_name' : 'nrappkit',
          'type' : 'static_library',

          'include_dirs' : [
              # EXTERNAL
              # INTERNAL
	      'src/event',
	      'src/log',
              'src/port/generic/include',
	      'src/registry',
	      'src/share',
	      'src/stats',
	      'src/util',
	      'src/util/libekr',

	      # Mozilla, hopefully towards the end
             '$(DEPTH)/dist/include',
          ],

          'sources' : [
	      # Shared
#              './src/share/nr_api.h',
              './src/share/nr_common.h',
#              './src/share/nr_dynlib.h',
              './src/share/nr_reg_keys.h',
#              './src/share/nr_startup.c',
#              './src/share/nr_startup.h',
#              './src/share/nrappkit_static_plugins.c',
	       './src/port/generic/include'

              # libekr
              './src/util/libekr/assoc.h',
              './src/util/libekr/debug.c',
              './src/util/libekr/debug.h',
              './src/util/libekr/r_assoc.c',
              './src/util/libekr/r_assoc.h',
#              './src/util/libekr/r_assoc_test.c',
              './src/util/libekr/r_bitfield.c',
              './src/util/libekr/r_bitfield.h',
              './src/util/libekr/r_common.h',
              './src/util/libekr/r_crc32.c',
              './src/util/libekr/r_crc32.h',
              './src/util/libekr/r_data.c',
              './src/util/libekr/r_data.h',
              './src/util/libekr/r_defaults.h',
              './src/util/libekr/r_errors.c',
              './src/util/libekr/r_errors.h',
              './src/util/libekr/r_includes.h',
              './src/util/libekr/r_list.c',
              './src/util/libekr/r_list.h',
              './src/util/libekr/r_macros.h',
              './src/util/libekr/r_memory.c',
              './src/util/libekr/r_memory.h',
              './src/util/libekr/r_replace.c',
              './src/util/libekr/r_thread.h',
              './src/util/libekr/r_time.c',
              './src/util/libekr/r_time.h',
              './src/util/libekr/r_types.h',
              './src/util/libekr/debug.c',
              './src/util/libekr/debug.h',

	      # Utilities
              './src/util/byteorder.c',
              './src/util/byteorder.h',
              #'./src/util/escape.c',
              #'./src/util/escape.h',
              #'./src/util/filename.c',
              #'./src/util/getopt.c',
              #'./src/util/getopt.h',
              './src/util/hex.c',
              './src/util/hex.h',
	      #'./src/util/mem_util.c',
              #'./src/util/mem_util.h',
              #'./src/util/mutex.c',
              #'./src/util/mutex.h',
              './src/util/p_buf.c',
              './src/util/p_buf.h',
              #'./src/util/ssl_util.c',
              #'./src/util/ssl_util.h',
              './src/util/util.c',
              './src/util/util.h',
              #'./src/util/util_db.c',
              #'./src/util/util_db.h',

	      # Events
#              './src/event/async_timer.c',
              './src/event/async_timer.h',
#              './src/event/async_wait.c',
              './src/event/async_wait.h',
              './src/event/async_wait_int.h',

	      # Logging
              './src/log/r_log.c',
              './src/log/r_log.h',
              #'./src/log/r_log_plugin.c',

	      # Registry
              './src/registry/c2ru.c',
              './src/registry/c2ru.h',
              #'./src/registry/mod_registry/mod_registry.c',
              #'./src/registry/nrregctl.c',
              #'./src/registry/nrregistryctl.c',
              './src/registry/registry.c',
              './src/registry/registry.h',
              './src/registry/registry_int.h',
              './src/registry/registry_local.c',
              #'./src/registry/registry_plugin.c',
              './src/registry/registry_vtbl.h',
              './src/registry/registrycb.c',
              #'./src/registry/registryd.c',
              #'./src/registry/regrpc.h',
              #'./src/registry/regrpc_client.c',
              #'./src/registry/regrpc_client.h',
              #'./src/registry/regrpc_client_cb.c',
              #'./src/registry/regrpc_clnt.c',
              #'./src/registry/regrpc_server.c',
              #'./src/registry/regrpc_svc.c',
              #'./src/registry/regrpc_xdr.c',

	      # Statistics
              #'./src/stats/nrstats.c',
              #'./src/stats/nrstats.h',
              #'./src/stats/nrstats_app.c',
              #'./src/stats/nrstats_int.h',
              #'./src/stats/nrstats_memory.c',
          ],
          
          'defines' : [
              'SANITY_CHECKS',
	      'R_PLATFORM_INT_TYPES=\'<stdint.h>\'',
	      'R_DEFINED_INT2=int16_t',
	      'R_DEFINED_UINT2=uint16_t',
	      'R_DEFINED_INT4=int32_t',
	      'R_DEFINED_UINT4=uint32_t',
	      'R_DEFINED_INT8=int64_t',
	      'R_DEFINED_UINT8=uint64_t',
          ],
          
          'conditions' : [
              ## Mac
              [ 'OS == "mac"', {
                'cflags_mozilla': [
                    '-Wall',
                    '-Wno-parentheses',
                    '-Wno-strict-prototypes',
                    '-Wmissing-prototypes',
                 ],
                 'defines' : [
                     'DARWIN',
                     'HAVE_LIBM=1',
                     'HAVE_STRDUP=1',
                     'HAVE_STRLCPY=1',
                     'HAVE_SYS_TIME_H=1',
                     'HAVE_VFPRINTF=1',
                     'NEW_STDIO'
                     'RETSIGTYPE=void',
                     'TIME_WITH_SYS_TIME_H=1',
                     '__UNUSED__="__attribute__((unused))"',
                 ],

		 'include_dirs': [
		     'src/port/darwin/include'
		 ],
		 
		 'sources': [
              	      './src/port/darwin/include/csi_platform.h',
		 ],
              }],
              
              ## Win
              [ 'OS == "win"', {
                 'defines' : [
                     'WIN',
                     '__UNUSED__=""',
                     'HAVE_STRDUP=1',
                     'NO_REG_RPC'
                 ],

		 'include_dirs': [
		     'src/port/win32/include'
		 ],

		 'sources': [
              	      './src/port/win32/include/csi_platform.h',
		 ],
              }],
              ## Linux
              [ '(OS == "linux") or (OS == "android")', {
                 'cflags_mozilla': [
                     '-Wall',
                     '-Wno-parentheses',
                     '-Wno-strict-prototypes',
                     '-Wmissing-prototypes',
                 ],
                 'defines' : [
                     'LINUX',
                     'HAVE_LIBM=1',
                     'HAVE_STRDUP=1',
                     'HAVE_STRLCPY=1',
                     'HAVE_SYS_TIME_H=1',
                     'HAVE_VFPRINTF=1',
                     'NEW_STDIO'
                     'RETSIGTYPE=void',
                     'TIME_WITH_SYS_TIME_H=1',
                     'NO_REG_RPC=1',
                     '__UNUSED__="__attribute__((unused))"',
                 ],

		 'include_dirs': [
		     'src/port/linux/include'
		 ],
		 'sources': [
              	      './src/port/linux/include/csi_platform.h',
		 ],
              }]
          ]
      }]
}


