export let data = { ifindex: 7, writes: [], config: { '1': 'TEST12345678', '9': 1 } };
export function status(device) { return { ifindex: data.ifindex, device }; }
export function config_list() {
	let result = [];
	for (let key, value in data.config) push(result, { key: int(key), value });
	return result;
}
export function config_set(device, key, value) {
	push(data.writes, { device, key, value });
	data.config[`${key}`] = value;
	if (key == 3) { data.config['17'] = value; data.config['18'] = value; }
	return true;
}
export function error() { return { errno: 5, message: 'injected failure' }; }
