export let data = {
	state: { netdev: 'pon0', ifindex: 7, mode: 2, capabilities: 15, max_links: 8 },
	links: [ { id: 1, enabled: 0 }, { id: 2, enabled: 0 } ],
	writes: [], fail_id: 0
};
export function status() { return data.state; }
export function links() { return map(data.links, l => ({ id: l.id, enabled: l.enabled })); }
export function error() { return { errno: 5, message: 'injected failure' }; }
export function link_set(netdev, config) {
	if (config.id == data.fail_id) return null;
	push(data.writes, { id: config.id, enabled: config.enabled });
	for (let link in data.links)
		if (link.id == config.id) link.enabled = config.enabled;
	return true;
}
