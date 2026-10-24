'use strict';
import { data as config } from 'uci';
import { data as pon } from 'xpon';
import { data as channel } from 'xpon_omci';
import { timers } from 'uloop';

let api = controller['xpon.agent'];
let legacy = controller['omci.agent'];
function request(args) {
	return { args: args || {}, reply: (data, status) => { } };
}
function check(ok, message) {
	if (!ok) die(`FAIL: ${message}\n`);
	print(`PASS: ${message}\n`);
}
check(length(channel.writes) == 0, 'unchanged OMCI setup is idempotent at startup');
config.main.serial = 'TEST87654321';
api.apply.call(request());
check(length(channel.writes) == 1 && channel.writes[0].device == 5,
	'apply resolves the configured OMCI device ID');
api.apply.call(request());
check(length(channel.writes) == 1, 'repeated apply does not rewrite unchanged OMCI values');
check(legacy.config_set == api.omci_config_set, 'legacy object shares the same controller');

pon.state.mode = 4;
config.links = [ { '.name': 'one', id: '1', enabled: '1' } ];
api.apply.call(request());
check(length(pon.writes) == 1 && length(channel.writes) == 1,
	'EPON setup applies local links without sending OMCI configuration');
api.apply.call(request());
check(length(pon.writes) == 1, 'repeated EPON apply preserves registration');
config.links = [ { id: '1', enabled: '0' }, { id: '1', enabled: '1' } ];
api.apply.call(request());
check(length(pon.writes) == 1, 'duplicate link IDs are rejected before changing hardware');
config.links = [ { id: '1', enabled: '0' }, { id: '3', enabled: '1' } ];
api.apply.call(request());
check(length(pon.writes) == 1, 'missing link objects are detected before the first mutation');
config.links = [ { id: '1', enabled: '0' }, { id: '2', enabled: '1' } ];
pon.fail_id = 2;
api.apply.call(request());
check(pon.links[0].enabled == 1 && length(pon.writes) == 3,
	'failed multi-link setup restores the previous administrative state');

pon.fail_id = 0;
pon.state.mode = 2;
channel.ifindex = 99;
config.main.serial = 'TEST00000001';
api.apply.call(request());
check(length(channel.writes) == 1, 'mismatched OMCI and PON interfaces are rejected');
channel.ifindex = 7;
api.apply.call(request());
let before = length(channel.writes);
channel.config['1'] = 'TESTRESET000';
pon.state.ifindex = 8;
channel.ifindex = 8;
for (let i = 0; i < 16; i++) timers[0]();
check(length(channel.writes) == before + 1,
	'changed backend ifindex reapplies unchanged UCI configuration');
config.main.version = 'legacy';
config.main.software_version_0 = 'explicit';
channel.config['17'] = 'explicit';
api.apply.call(request());
check(channel.config['16'] == 'legacy' && channel.config['18'] == 'legacy' &&
	channel.config['17'] == 'explicit', 'legacy version fills missing fields without overwriting explicit versions');
before = length(channel.writes);
api.apply.call(request());
check(length(channel.writes) == before, 'legacy and explicit version setup remains idempotent');
print('Backend tests passed\n');
