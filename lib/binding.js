const { ccallThrowAsync } = require('./util');

const ops = [
	['open', 4],
	['close', 1],
	['rmdir', 2],
	['mkdir', 3],
	['readdir', 2],
	['read', 5],
	['write', 5],
	['link', 3],
	['symlink', 3],
	['readlink', 3],
	['rename', 3],
	['unlink', 2],
	['chmod', 2],
	['chown', 3],
	['chown', 3],
	['stat_ino', 1],
	['stat_i_mode', 1],
	['stat_i_links_count', 1],
	['stat_i_uid', 1],
	['stat_i_gid', 1],
	['stat_i_size', 1],
	['stat_i_blocks', 1],
	['stat_i_atime', 1],
	['stat_i_mtime', 1],
	['stat_i_ctime', 1],
	['stat_blocksize', 1],
];

const bind = (op, numArgs) => {
	const id = 'node_ext2fs_' + op;
	// args will always be numbers
	const argsDes = (new Array(numArgs)).fill('number');
	return (...args) => {
		try {
			return ccallThrowAsync(id, 'number', argsDes, args);
		} catch (err) {
			err.message += '\nError calling ' + op;
			throw err;
		}
	};
};
module.exports = ops.reduce((acc, [name, numArgs]) => {
	acc[name] = bind(name, numArgs);
	return acc;
}, {});