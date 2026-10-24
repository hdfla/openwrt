'use strict';
'require view';
'require form';
'require uci';
'require poll';
'require dom';
'require xpon.rpc as xpon';

function snapshot() {
	return xpon.ponStatus().then(function(state) {
		if (state.mode !== 4 && state.mode !== 8)
			return { error: _('The selected PON device is not running in EPON mode.') };
		return xpon.links(state.netdev).then(function(links) {
			return { state: state, links: links };
		});
	}).catch(function(error) { return { error: error.message }; });
}

function runtime(data) {
	if (data.error)
		return E('div', { 'class': 'alert-message warning' }, data.error);
	var names = [ _('Disabled'), _('Registering'), _('Registered') ];
	var rows = [ E('tr', {}, [ _('Local link'), _('Assigned LLID'), _('MPCP state'),
		_('Enabled'), _('OAM RX frames'), _('OAM TX frames') ].map(function(h) { return E('th', {}, h); })) ];
	data.links.forEach(function(link) {
		rows.push(E('tr', {}, [ link.id, link.link_state === 2 ? link.wire_id : '-',
			names[link.link_state] || _('Unknown'), link.enabled ? _('Yes') : _('No'),
			link.rx_oam, link.tx_oam ].map(function(v) { return E('td', {}, v == null ? '-' : String(v)); })));
	});
	return E('div', {}, [
		E('p', {}, _('Device: %s').format(data.state.netdev)),
		E('table', { 'class': 'table' }, rows),
		E('p', {}, _('Counters are lifetime software OAM transport counters. MPCP registration does not indicate OAM discovery, authentication or service readiness.')),
		E('p', {}, _('This kernel API does not yet expose in-kernel OAM profile or authentication configuration.'))
	]);
}

return view.extend({
	load: function() {
		return Promise.all([ uci.load('xpon'), snapshot() ]);
	},
	render: function(data) {
		var m = new form.Map('xpon', _('EPON'),
			_('Configure local registration instances. The OLT assigns wire LLIDs; registration and protocol processing remain in the kernel.'));
		var s = m.section(form.TypedSection, 'link', _('Local links'));
		s.anonymous = true;
		s.addremove = true;
		var o = s.option(form.Value, 'id', _('Local link ID'));
		o.datatype = data[1].state ? 'range(1,%d)'.format(data[1].state.max_links) : 'min(1)';
		o.rmempty = false;
		o = s.option(form.Flag, 'enabled', _('Enable registration'));
		o.default = '1';
		o.rmempty = false;
		poll.add(function() {
			return snapshot().then(function(next) {
				var node = document.getElementById('epon-runtime');
				if (node) dom.content(node, runtime(next));
			});
		}, 5);
		return m.render().then(function(form) {
			return E('div', {}, [ E('div', { id: 'epon-runtime' }, runtime(data[1])), form ]);
		});
	}
});
