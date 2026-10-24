'use strict';
'require view';
'require poll';
'require dom';
'require xpon.rpc as xpon';

function text(value) {
	return value == null ? '-' : String(value);
}

function flag(value) {
	return value == null ? _('Unavailable') : value ? _('Yes') : _('No');
}

function table(headers, items, fields) {
	var rows = [ E('tr', {}, headers.map(function(h) { return E('th', {}, h); })) ];
	items.forEach(function(item) {
		rows.push(E('tr', {}, fields.map(function(f) { return E('td', {}, text(item[f])); })));
	});
	if (!items.length)
		rows.push(E('tr', {}, [ E('td', { colspan: headers.length }, _('No objects reported')) ]));
	return E('table', { 'class': 'table' }, rows);
}

function snapshot() {
	return xpon.ponStatus().then(function(state) {
		function read(fn) {
			return fn(state.netdev).then(function(value) { return { value: value }; })
				.catch(function(error) { return { error: error.message }; });
		}
		return Promise.all([ read(xpon.endpoints), read(xpon.unis) ]).then(function(parts) {
			return { state: state, endpoints: parts[0], unis: parts[1] };
		});
	}).catch(function(error) { return { error: error.message }; });
}

function runtime(data) {
	if (data.error)
		return E('div', { 'class': 'alert-message warning' }, data.error);
	var s = data.state;
	var rows = [
		[ _('Device'), text(s.netdev) ],
		[ _('Interface index'), text(s.ifindex) ],
		[ _('PON mode'), text(s.mode_name) ],
		[ _('Registration'), text(s.registration_name) ],
		[ _('Carrier'), flag(s.carrier) ],
		[ _('Signal detected'), flag(s.signal_detect) ],
		[ _('Loss of signal'), flag(s.los) ],
		[ _('ONU type'), text(s.onu_type_name) ],
		[ _('Ethernet UNIs'), text(s.uni_count) ]
	];
	return E('div', {}, [
		E('table', { 'class': 'table' }, rows.map(function(row) {
			return E('tr', {}, [ E('th', {}, row[0]), E('td', {}, row[1]) ]);
		})),
		E('h3', {}, _('Endpoints')),
		data.endpoints.error ? E('p', {}, data.endpoints.error) :
			table([ _('ID'), _('Interface'), _('Type'), _('Wire ID'), _('Scheduler'), _('Queue') ],
				(data.endpoints.value || []).map(function(ep) {
					return Object.assign({}, ep, { type: ep.type === 0 ? 'GEM' : ep.type === 1 ? 'LLID' : ep.type });
				}), [ 'id', 'name', 'type', 'wire_id', 'scheduler_id', 'queue' ]),
		E('h3', {}, _('Ethernet UNIs')),
		data.unis.error ? E('p', {}, data.unis.error) :
			table([ _('ID'), _('Interface'), _('Enabled'), _('Carrier') ],
				(data.unis.value || []).map(function(uni) {
					return Object.assign({}, uni, { enabled: flag(uni.enabled), carrier: flag(uni.carrier) });
				}), [ 'id', 'name', 'enabled', 'carrier' ])
	]);
}

return view.extend({
	load: snapshot,
	render: function(data) {
		poll.add(function() {
			return snapshot().then(function(next) {
				var node = document.getElementById('xpon-runtime');
				if (node) dom.content(node, runtime(next));
			});
		}, 5);
		return E('div', {}, [
			E('h2', {}, _('xPON status')),
			E('div', { id: 'xpon-runtime' }, runtime(data))
		]);
	},
	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
