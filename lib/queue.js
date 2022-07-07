'use strict';

let running = false;
const queue = [];

async function run() {
	running = true;
	if (queue.length === 0) {
		running = false;
		return;
	}
	const { fn, args, resolve, reject } = queue.shift();
	try {
		resolve(await fn(...args));
	} catch (error) {
		reject(error);
	} finally {
		await run();
	}
}

exports.addOperation = function(fn, args) {
	return new Promise((resolve, reject) => {
		queue.push({ fn, args, resolve, reject });
		if (!running) {
			run();
		}
	});
};
