// Injected inside the verified UI globals module. Does not modify gameplay state.
function EFPInit() {
    if (window.__EFPReporter) return;
    const session = Date.now().toString(36) + Math.random().toString(36).slice(2, 10);
    let sequence = 0;
    let pending = null;
    let stopped = false;
    function snapshot() {
        let raw = 'unknown';
        let state = 'unknown';
        try {
            if (stack.state(StateStacks_programFlowStack.name) !== 'game' || !stack.active('game')) {
                return {state: 'protected', raw: 'program_flow'};
            }
            raw = String(stack.state('game'));
            state = raw === 'exploration' ? 'exploration' : raw === 'combat' ? 'combat' : 'protected';
            if (typeof challengeMenuData !== 'undefined') {
                const challenge = core_get(challengeMenuData);
                if (challenge && challenge.state !== undefined && challenge.state !== 8) state = 'protected';
            }
            if (typeof gameplayMenuData !== 'undefined') {
                const menu = core_get(gameplayMenuData);
                if (menu && menu.open) state = 'protected';
            }
        } catch (_) { state = 'unknown'; }
        return {state, raw};
    }
    function poll() {
        if (stopped) return;
        const sample = snapshot();
        const request = new XMLHttpRequest();
        pending = request;
        let finished = false;
        function complete() {
            if (finished) return;
            finished = true;
            if (pending === request) pending = null;
            if (!stopped) setTimeout(poll, 50);
        }
        request.onload = complete;
        request.onerror = complete;
        request.ontimeout = complete;
        try {
            request.open('GET', 'coui://base/__efp_state__.json?session=' + encodeURIComponent(session) +
                '&seq=' + (++sequence) + '&state=' + sample.state + '&raw=' + encodeURIComponent(sample.raw), true);
            request.timeout = 250;
            request.send();
        } catch (_) { complete(); }
        // Some embedded UI engines do not implement XMLHttpRequest.timeout.
        setTimeout(function () {
            if (!finished) { try { request.abort(); } catch (_) {} complete(); }
        }, 250);
    }
    window.__EFPReporter = {stop: function () {
        stopped = true;
        if (pending) { try { pending.abort(); } catch (_) {} }
    }};
    poll();
}
