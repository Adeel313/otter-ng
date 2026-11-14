// SPDX-License-Identifier: GPL-3.0-or-later
// Run with: node tests/imageVisibility.test.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname,
    '../src/modules/backends/web/qtwebengine/resources/imageVisibility.js'), 'utf8');

function page(ready = true) {
    const styles = new Map();
    const listeners = [];
    const manualImage = {reloaded: true, classList: {remove() { manualImage.reloaded = false; }}};
    const document = {
        documentElement: ready ? {} : null,
        head: {appendChild(style) { styles.set(style.id, style); }},
        getElementById(id) { return styles.get(id); },
        createElement(tag) {
            assert.equal(tag, 'style');
            return {remove() { styles.delete(this.id); }};
        },
        querySelectorAll() { return manualImage.reloaded ? [manualImage] : []; },
        addEventListener(name, callback) {
            assert.equal(name, 'DOMContentLoaded');
            listeners.push(callback);
        }
    };
    const context = vm.createContext({document, window: {}});
    return {
        styles,
        manualImage,
        apply(hidden) { vm.runInContext(source.replace('%1', String(hidden)), context); },
        ready() { document.documentElement = {}; listeners.splice(0).forEach(callback => callback()); }
    };
}

const tab = page();
tab.apply(true);
assert.equal(tab.styles.size, 1);
assert.equal(tab.manualImage.reloaded, false, 'entering no-images mode must hide previously reloaded images');
const rule = [...tab.styles.values()][0].textContent;
assert.match(rule, /img:not\(\.otter-image-reloaded\)/);
assert.match(rule, /background-image: none/);
tab.apply(true);
assert.equal(tab.styles.size, 1, 'repeated updates must not accumulate styles');
tab.manualImage.reloaded = true;
tab.apply(true);
assert.equal(tab.manualImage.reloaded, true, 'an explicit image reload remains visible until the mode changes');
tab.apply(false);
assert.equal(tab.styles.size, 0, 'show/cached modes must restore images');

const otherTab = page();
otherTab.apply(false);
tab.apply(true);
assert.equal(otherTab.styles.size, 0, 'image visibility must be isolated per document');

const loadingTab = page(false);
loadingTab.apply(true);
loadingTab.apply(false);
loadingTab.ready();
assert.equal(loadingTab.styles.size, 0, 'a stale DOM ready callback must not restore an old mode');
loadingTab.apply(true);
assert.equal(loadingTab.styles.size, 1);
console.log('PASS image mode transitions, tab isolation, duplicate updates, and early-navigation changes');
