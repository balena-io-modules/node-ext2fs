'use strict';

let running = false;
const queue = [];

function run() {
	if (queue.length === 0) {
		running = false;
		return;
	}
	running = true;
	const { method, args } = queue.shift();
	const cb = args.pop();
	method(...args, (err, result) => {
		cb(err, result);
		run();
	});
}

exports.addOperation = function(method, args) {
	queue.push({ method: method, args: args });
	if (!running) {
		run();
	}
};
