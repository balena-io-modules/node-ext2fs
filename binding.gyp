{
	'variables': { 'target_arch%': 'x64' },
	'target_defaults': {
		'default_configuration': 'Debug',
		'configurations': {
			'Debug': {
				'defines': [ 'DEBUG', '_DEBUG' ],
				'msvs_settings': {
					'VCCLCompilerTool': {
						'RuntimeLibrary': 1, # static debug
					},
				},
			},
			'Release': {
				'defines': [ 'NDEBUG' ],
				'msvs_settings': {
					'VCCLCompilerTool': {
						'RuntimeLibrary': 0, # static release
					},
				},
			}
		},
		'msvs_settings': {
			'VCLinkerTool': {
				'GenerateDebugInformation': 'true',
				'ImageHasSafeExceptionHandlers': 'false',
				'LinkTimeCodeGeneration': 0,
				# The symbols that end up being undefined are not used by this
				# library so we don't care
				'ForceFileOutput': 'UndefinedSymbolOnly',
			},
		},
		"cflags_cc": [ '-fpermissive' ],
	},

	'targets': [
		{
			'target_name': 'ext2fs',
			'conditions': [
				['OS=="win"', {
					'actions': [
						{
							'message': 'Copying Makefile...',
							'action_name': "libext2fs_makefile",
							'inputs': [
								"config/win/Makefile"
							],
							'outputs': [
								"deps/e2fsprogs/lib/ext2fs/Makefile"
							],
							'action': [ 'COPY', 'config/win/Makefile', 'deps/e2fsprogs/lib/ext2fs/Makefile' ],
						},
						{
							'message': 'Building libext2fs...',
							'action_name': "libext2fs_build",
							'inputs': [
								"deps/e2fsprogs/"
							],
							'outputs': [
								"deps/e2fsprogs/lib/ext2fs/libext2fs.a"
							],
							'action': [ 'mingw32-make', '-C', 'deps/e2fsprogs/lib/ext2fs' ]
						}
					]
				}, {
					'product_prefix': 'lib',
					'type': 'static_library',
					'sources': [
						'deps/e2fsprogs/lib/ext2fs/alloc.c',
						'deps/e2fsprogs/lib/ext2fs/alloc_sb.c',
						'deps/e2fsprogs/lib/ext2fs/alloc_stats.c',
						'deps/e2fsprogs/lib/ext2fs/alloc_tables.c',
						'deps/e2fsprogs/lib/ext2fs/atexit.c',
						'deps/e2fsprogs/lib/ext2fs/badblocks.c',
						'deps/e2fsprogs/lib/ext2fs/bb_compat.c',
						'deps/e2fsprogs/lib/ext2fs/bb_inode.c',
						'deps/e2fsprogs/lib/ext2fs/bitmaps.c',
						'deps/e2fsprogs/lib/ext2fs/bitops.c',
						'deps/e2fsprogs/lib/ext2fs/blkmap64_ba.c',
						'deps/e2fsprogs/lib/ext2fs/blkmap64_rb.c',
						'deps/e2fsprogs/lib/ext2fs/blknum.c',
						'deps/e2fsprogs/lib/ext2fs/block.c',
						'deps/e2fsprogs/lib/ext2fs/bmap.c',
						'deps/e2fsprogs/lib/ext2fs/check_desc.c',
						'deps/e2fsprogs/lib/ext2fs/closefs.c',
						'deps/e2fsprogs/lib/ext2fs/crc16.c',
						'deps/e2fsprogs/lib/ext2fs/crc32c.c',
						'deps/e2fsprogs/lib/ext2fs/csum.c',
						'deps/e2fsprogs/lib/ext2fs/dblist.c',
						'deps/e2fsprogs/lib/ext2fs/dblist_dir.c',
						'deps/e2fsprogs/lib/ext2fs/dirblock.c',
						'deps/e2fsprogs/lib/ext2fs/dirhash.c',
						'deps/e2fsprogs/lib/ext2fs/dir_iterate.c',
						'deps/e2fsprogs/lib/ext2fs/dupfs.c',
						'deps/e2fsprogs/lib/ext2fs/expanddir.c',
						'deps/e2fsprogs/lib/ext2fs/ext_attr.c',
						'deps/e2fsprogs/lib/ext2fs/extent.c',
						'deps/e2fsprogs/lib/ext2fs/fallocate.c',
						'deps/e2fsprogs/lib/ext2fs/fileio.c',
						'deps/e2fsprogs/lib/ext2fs/finddev.c',
						'deps/e2fsprogs/lib/ext2fs/flushb.c',
						'deps/e2fsprogs/lib/ext2fs/freefs.c',
						'deps/e2fsprogs/lib/ext2fs/gen_bitmap64.c',
						'deps/e2fsprogs/lib/ext2fs/gen_bitmap.c',
						'deps/e2fsprogs/lib/ext2fs/get_num_dirs.c',
						'deps/e2fsprogs/lib/ext2fs/get_pathname.c',
						'deps/e2fsprogs/lib/ext2fs/getsectsize.c',
						'deps/e2fsprogs/lib/ext2fs/getsize.c',
						'deps/e2fsprogs/lib/ext2fs/i_block.c',
						'deps/e2fsprogs/lib/ext2fs/icount.c',
						'deps/e2fsprogs/lib/ext2fs/imager.c',
						'deps/e2fsprogs/lib/ext2fs/ind_block.c',
						'deps/e2fsprogs/lib/ext2fs/initialize.c',
						'deps/e2fsprogs/lib/ext2fs/inline.c',
						'deps/e2fsprogs/lib/ext2fs/inline_data.c',
						'deps/e2fsprogs/lib/ext2fs/inode.c',
						'deps/e2fsprogs/lib/ext2fs/io_manager.c',
						'deps/e2fsprogs/lib/ext2fs/ismounted.c',
						'deps/e2fsprogs/lib/ext2fs/link.c',
						'deps/e2fsprogs/lib/ext2fs/llseek.c',
						'deps/e2fsprogs/lib/ext2fs/lookup.c',
						'deps/e2fsprogs/lib/ext2fs/mkdir.c',
						'deps/e2fsprogs/lib/ext2fs/mkjournal.c',
						'deps/e2fsprogs/lib/ext2fs/mmp.c',
						'deps/e2fsprogs/lib/ext2fs/namei.c',
						'deps/e2fsprogs/lib/ext2fs/native.c',
						'deps/e2fsprogs/lib/ext2fs/newdir.c',
						'deps/e2fsprogs/lib/ext2fs/openfs.c',
						'deps/e2fsprogs/lib/ext2fs/progress.c',
						'deps/e2fsprogs/lib/ext2fs/punch.c',
						'deps/e2fsprogs/lib/ext2fs/qcow2.c',
						'deps/e2fsprogs/lib/ext2fs/rbtree.c',
						'deps/e2fsprogs/lib/ext2fs/read_bb.c',
						'deps/e2fsprogs/lib/ext2fs/read_bb_file.c',
						'deps/e2fsprogs/lib/ext2fs/res_gdt.c',
						'deps/e2fsprogs/lib/ext2fs/rw_bitmaps.c',
						'deps/e2fsprogs/lib/ext2fs/sha512.c',
						'deps/e2fsprogs/lib/ext2fs/swapfs.c',
						'deps/e2fsprogs/lib/ext2fs/symlink.c',
						'deps/e2fsprogs/lib/ext2fs/tdb.c',
						'deps/e2fsprogs/lib/ext2fs/unlink.c',
						'deps/e2fsprogs/lib/ext2fs/unix_io.c',
						'deps/e2fsprogs/lib/ext2fs/valid_blk.c',
						'deps/e2fsprogs/lib/ext2fs/version.c',
						'deps/e2fsprogs/lib/ext2fs/write_bb_file.c',
					],
					'include_dirs': [
							'config/common',

							'deps/e2fsprogs',
							'deps/e2fsprogs/lib',
							# platform and arch-specific headers
							'config/<(OS)/<(target_arch)'
					],
					'defines': [
							'HAVE_CONFIG_H',
					],
				}]
			]
		},
		{
			"target_name": "bindings",
			"sources": [
				"src/node_ext2fs.cc",
				"src/async.cc",
				"src/js_io.cc",
				"src/bindings.cc"
			],

			'conditions': [
				['OS=="mac"', {
					"xcode_settings": {
						"OTHER_CPLUSPLUSFLAGS": [
							"-stdlib=libc++"
						],
						"OTHER_LDFLAGS": [
							"-stdlib=libc++"
						]
					}
				},
				'OS=="win"', {
					"libraries": [
						"../deps/e2fsprogs/lib/ext2fs/libext2fs.a",
					],
				}]
			],
			'dependencies': [ 'ext2fs' ],
			"include_dirs": [
				"<!(node -e \"require('nan')\")"
			],
		}
	]
}
