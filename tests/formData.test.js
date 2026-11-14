// SPDX-License-Identifier: GPL-3.0-or-later
// Run with: node tests/formData.test.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/modules/backends/web/qtwebengine/resources/formData.js', 'utf8');
const listeners = {};
const messages = [];
const timers = new Map();
let timerId = 0;
const windowListeners = {};
class Input {
    constructor(attributes = {}, value = '') {
        this.attributes = attributes;
        this.name = attributes.name || '';
        this.id = attributes.id || '';
        this.type = attributes.type || 'text';
        this.tagName = 'INPUT';
        this.value = value;
    }
    getAttribute(name) { return this.attributes[name] || null; }
    getBoundingClientRect() { return {left: 10, top: 20, width: 200, height: 30}; }
    matches() { return true; }
}
const unnamed = new Input({autocomplete: 'section-contact email webauthn'});
const empty = new Input({name: 'empty'});
const email = new Input({name: 'contact', type: 'email'}, 'person@example.com');
const password = new Input({name: 'password', type: 'password'}, 'secret');
const off = new Input({name: 'off', autocomplete: 'off'}, 'private');
const external = new Input({name: 'external'}, 'associated with form attribute');
let submit;
const form = {
    elements: [email, empty, password, off, external],
    addEventListener(name, callback) { assert.equal(name, 'submit'); submit = callback; }
};
const environment = {
    window: {location: {href: 'https://example.com/form'}, addEventListener(name, callback) { windowListeners[name] = callback; }},
    setTimeout(callback) { timers.set(++timerId, callback); return timerId; },
    clearTimeout(id) { timers.delete(id); },
    document: {
        documentElement: {},
        querySelectorAll() { return [form]; },
        addEventListener(name, callback) { listeners[name] = callback; }
    },
    HTMLInputElement: Input, HTMLTextAreaElement: class {}, HTMLSelectElement: class {},
    MutationObserver: class { observe() {} },
    console: {info(message) {
        const [kind, token, payload] = message.split(':');
        messages.push({kind, data: JSON.parse(Buffer.from(payload, 'base64').toString())});
    }},
    btoa: text => Buffer.from(text, 'binary').toString('base64'),
    unescape, encodeURIComponent
};
vm.runInNewContext(source, environment);
listeners.focusin({target: unnamed});
assert.equal(messages.at(-1).data.name, 'email', 'unnamed autocomplete fields can request profile values');
assert.equal(messages.at(-1).data.autocomplete, 'section-contact email', 'webauthn is not the field semantic');
listeners.focusin({target: email});
assert.equal(messages.at(-1).data.autocomplete, 'email', 'email inputs infer their semantic from type');
listeners.input({target: empty});
assert.equal(messages.at(-1).data.value, '', 'clearing an input refreshes suggestions');
submit();
const saved = messages.at(-1).data.fields;
assert.deepEqual(saved.map(field => field.name), ['contact', 'external'], 'submit excludes empty and sensitive fields and includes externally associated controls');
assert.equal(saved[0].autocomplete, 'email');
// New values must save without a blur or native form submission.
const typed = new Input({name: 'notes'}, 'newly typed value');
listeners.input({target: typed});
assert.equal(timers.size, 1);
[...timers.values()][0]();
assert.equal(messages.at(-1).kind, 'otter-form-save');
assert.equal(messages.at(-1).data.fields[0].value, 'newly typed value');
typed.value = 'save on blur';
listeners.input({target: typed});
listeners.focusout({target: typed});
assert.equal(messages.at(-1).data.fields[0].value, 'save on blur');
typed.value = 'save before navigating';
listeners.input({target: typed});
windowListeners.pagehide();
assert.equal(messages.at(-1).data.fields[0].value, 'save before navigating');
assert.equal(timers.size, 0);
const count = messages.length;
listeners.change({target: {}});
listeners.input({target: password});
listeners.input({target: off});
assert.equal(messages.length, count, 'non-controls and sensitive or opted-out inputs must not save');
console.log('PASS form capture, unnamed fields, semantic matching, and empty-input suggestions');

// Exercise the exact selection script shipped by the backend, including the
// native value setter needed by controlled inputs and the stale-value guard.
const backend = fs.readFileSync('src/modules/backends/web/qtwebengine/QtWebEnginePage.cpp', 'utf8');
const selection = backend.match(/const QString script\(QStringLiteral\("(\(function\(a\).*?)"\)\.arg\(valuesJson\)\);/)[1];
class ControlledInput extends Input {
    get value() { return this.currentValue || ''; }
    set value(value) { this.currentValue = value; }
    dispatchEvent(event) { this.events.push(event.type); }
}
const controlled = new ControlledInput({autocomplete: 'email'});
controlled.events = [];
const selectionContext = {document: {activeElement: controlled, hasFocus() { return true; }}, Event: class {constructor(type) { this.type = type; }}};
function select(prefix, value) {
    vm.runInNewContext(selection.replace('%1', JSON.stringify(['email', prefix, value, 'email'])), selectionContext);
}
select('', 'selected@example.com');
assert.equal(controlled.value, 'selected@example.com');
assert.deepEqual(controlled.events, ['input', 'change']);
select('', 'stale@example.com');
assert.equal(controlled.value, 'selected@example.com', 'stale suggestions must not overwrite newer input');
console.log('PASS native suggestion selection, change events, and stale-value guard');

const previousInputListener = listeners.input;
vm.runInNewContext(source, environment);
assert.equal(listeners.input, previousInputListener, 'reinjection must not duplicate listeners');
const frameListeners = {};
const embedded = {
    ...environment,
    window: {location: {href: 'https://forms.example.net/embedded'}, addEventListener() {}},
    document: {...environment.document, addEventListener(name, callback) { frameListeners[name] = callback; }}
};
vm.runInNewContext(source, embedded);
const frameInput = new Input({name: 'email', type: 'email'}, 'frame@example.com');
frameListeners.input({target: frameInput});
assert.equal(messages.at(-1).data.url, 'https://forms.example.net/embedded', 'frame suggestions carry the form origin');
frameListeners.change({target: frameInput});
assert.equal(messages.at(-1).data.url, 'https://forms.example.net/embedded', 'frame saves carry the form origin');
assert.equal(messages.at(-1).data.fields[0].value, 'frame@example.com');
selectionContext.document.hasFocus = () => false;
controlled.value = '';
select('', 'wrong-frame@example.com');
assert.equal(controlled.value, '', 'selection must not modify an unfocused frame');
console.log('PASS embedded frame capture, source URLs, focus isolation, and duplicate-injection guard');
for (const [name, semantic] of [['custname', 'name'], ['custemail', 'email'], ['custtel', 'tel'], ['firstName', 'given-name']]) {
    listeners.focusin({target: new Input({name})});
    assert.equal(messages.at(-1).data.autocomplete, semantic);
}
const labelled = new Input({name: 'q3'});
labelled.labels = [{textContent: 'First Name'}];
listeners.focusin({target: labelled});
assert.equal(messages.at(-1).data.autocomplete, 'given-name');
console.log('PASS common contact field names, camel case, and visible labels');
