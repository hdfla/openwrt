export let timers = [];
export function interval(ms, callback) {
	push(timers, callback);
	return {};
}
