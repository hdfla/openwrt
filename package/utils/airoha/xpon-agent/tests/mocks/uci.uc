export let data = {
	main: { device: '5', netdev: 'pon0', enabled: '1', serial: 'TEST12345678' },
	links: []
};
export function cursor() {
	return {
		get_all: () => data.main,
		unload: () => null,
		set: (package, section, key, value) => { data.main[key] = value; },
		commit: () => true,
		foreach: (package, type, callback) => {
			for (let link in data.links) callback(link);
		}
	};
}
