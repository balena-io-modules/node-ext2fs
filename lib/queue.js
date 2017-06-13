'use strict';

const async = require('async');

const queue = async.queue(function(task, callback) {
	const cb = task.args.pop();
	task.method(...task.args, (err, value) => {
		cb(err, value);
		callback(err);
	});
}, 1);

exports.addOperation = function(method, args) {
	queue.push({ method: method, args: args });
};
