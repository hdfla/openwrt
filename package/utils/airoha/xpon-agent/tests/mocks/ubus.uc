export function connect() {
	return { listener: () => ({}), call: (object, method, args) => ({ object, method, args }) };
}
