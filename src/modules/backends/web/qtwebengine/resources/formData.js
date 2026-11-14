(function(window)
{
	if (window.__otterFormDataInstalled)
	{
		return;
	}
	window.__otterFormDataInstalled = true;

	const token = '%1';
	const pendingFields = new Set();
	let saveTimer;
	const allowedAutocomplete = new Set(['name', 'given-name', 'additional-name', 'family-name', 'honorific-prefix', 'honorific-suffix', 'nickname', 'organization', 'organization-title', 'street-address', 'address-line1', 'address-line2', 'address-line3', 'address-level1', 'address-level2', 'address-level3', 'country', 'country-name', 'postal-code', 'email', 'tel', 'tel-country-code', 'tel-national', 'tel-area-code', 'tel-local', 'tel-extension', 'bday', 'bday-day', 'bday-month', 'bday-year']);
	const excludedAutocomplete = new Set(['current-password', 'new-password', 'one-time-code', 'cc-name', 'cc-given-name', 'cc-additional-name', 'cc-family-name', 'cc-number', 'cc-exp', 'cc-exp-month', 'cc-exp-year', 'cc-csc', 'cc-type']);

	function send(kind, payload)
	{
		try
		{
			console.info('otter-form-' + kind + ':' + token + ':' + btoa(unescape(encodeURIComponent(JSON.stringify(payload)))));
		}
		catch (error)
		{
		}
	}

	function guessAutocomplete(field)
	{
		const descriptor = [field.name, field.id, field.getAttribute('aria-label'), field.getAttribute('placeholder'), Array.from(field.labels || []).map(function(label) { return label.textContent; }).join(' ')].join(' ').replace(/([a-z])([A-Z])/g, '$1 $2').toLowerCase();
		const patterns = [
			[/\b(first[ _-]?name|given[ _-]?name|fname)\b/, 'given-name'],
			[/\b(last[ _-]?name|family[ _-]?name|surname|lname)\b/, 'family-name'],
			[/\b(full[ _-]?name|your[ _-]?name|custname|name)\b/, 'name'],
			[/\b(e[ _-]?mail|email[ _-]?address|custemail)\b/, 'email'],
			[/\b(phone|mobile|telephone|tel|custtel)\b/, 'tel'],
			[/\b(address[ _-]?line[ _-]?1|street[ _-]?address|street)\b/, 'address-line1'],
			[/\b(address[ _-]?line[ _-]?2)\b/, 'address-line2'],
			[/\b(city|town|locality)\b/, 'address-level2'],
			[/\b(state|province|region)\b/, 'address-level1'],
			[/\b(zip|postal[ _-]?code|postcode)\b/, 'postal-code'],
			[/\b(country)\b/, 'country-name'],
			[/\b(company|organization|organisation|employer)\b/, 'organization']
		];

		const match = patterns.find(function(entry) { return entry[0].test(descriptor); });
		return match ? match[1] : '';
	}

	function fieldData(field, allowEmptyValue)
	{
		if (!(field instanceof HTMLInputElement || field instanceof HTMLTextAreaElement || field instanceof HTMLSelectElement))
		{
			return null;
		}

		const autocomplete = (field.getAttribute('autocomplete') || '').toLowerCase().trim();
		const parts = autocomplete.split(/\s+/);
		const detailParts = parts.filter(function(part) { return part !== 'webauthn'; });
		const detail = detailParts[detailParts.length - 1];
		const semantic = (detail && detail !== 'on') ? detail : (guessAutocomplete(field) || (['email', 'tel'].includes(field.type) ? field.type : ''));
		const type = (field.type || field.tagName).toLowerCase();
		const descriptor = [field.name, field.id, field.getAttribute('aria-label'), field.getAttribute('placeholder'), Array.from(field.labels || []).map(function(label) { return label.textContent; }).join(' ')].join(' ').replace(/([a-z])([A-Z])/g, '$1 $2').toLowerCase();
		const key = (field.name || field.id || field.getAttribute('aria-label') || field.getAttribute('placeholder') || (allowedAutocomplete.has(semantic) ? semantic : '')).trim();

		if (!key || key.length > 256 || field.disabled || field.readOnly || (!allowEmptyValue && !field.value) || field.value.length > 1024 || parts.includes('off') || excludedAutocomplete.has(semantic) || /credit.?card|card.?number|card.?security|\bcvv\b|\bcvc\b|security.?code|social.?security|\bssn\b/.test(descriptor) || ['password', 'hidden', 'file', 'checkbox', 'radio', 'submit', 'button', 'reset', 'image'].includes(type))
		{
			return null;
		}

		if (!allowedAutocomplete.has(semantic) && !['text', 'textarea', 'select-one', 'email', 'tel', 'url', 'search', 'number'].includes(type))
		{
			return null;
		}

		return { name: key, autocomplete: (semantic && !allowedAutocomplete.has(detail) ? semantic : detailParts.join(' ')), value: field.value, type: type };
	}

	function attachForm(form)
	{
		if (form.__otterFormDataListener)
		{
			return;
		}

		form.__otterFormDataListener = true;
		form.addEventListener('submit', function()
		{
			const fields = Array.from(form.elements).filter(function(field) { return field.matches('input, select, textarea'); }).map(function(field) { return fieldData(field, false); }).filter(Boolean).slice(0, 50);
			if (fields.length)
			{
				send('save', { url: window.location.href, fields: fields });
			}
		}, true);
	}

	document.querySelectorAll('form').forEach(attachForm);
	new MutationObserver(function(mutations)
	{
		mutations.forEach(function(mutation)
		{
			mutation.addedNodes.forEach(function(node)
			{
				if (node.nodeType === Node.ELEMENT_NODE)
				{
					if (node.matches('form')) attachForm(node);
					node.querySelectorAll('form').forEach(attachForm);
				}
			});
		});
	}).observe(document.documentElement, { childList: true, subtree: true });

	function suggest(field, allowEmptyValue)
	{
		if (!(field instanceof HTMLInputElement || field instanceof HTMLTextAreaElement || field instanceof HTMLSelectElement))
		{
			return;
		}

		const data = fieldData(field, allowEmptyValue);
		if (data)
		{
			const rectangle = field.getBoundingClientRect();
			send('suggest', { url: window.location.href, name: data.name, autocomplete: data.autocomplete, value: data.value, x: rectangle.left, y: rectangle.top, width: rectangle.width, height: rectangle.height });
		}
	}

	function saveField(field)
	{
		pendingFields.delete(field);
		const data = fieldData(field, false);
		if (data)
		{
			send('save', { url: window.location.href, fields: [data] });
		}
	}

	function flushPendingFields()
	{
		clearTimeout(saveTimer);
		Array.from(pendingFields).forEach(saveField);
	}

	document.addEventListener('input', function(event)
	{
		suggest(event.target, true);
		if (fieldData(event.target, true))
		{
			pendingFields.add(event.target);
			clearTimeout(saveTimer);
			saveTimer = setTimeout(flushPendingFields, 500);
		}
	}, true);

	document.addEventListener('focusin', function(event)
	{
		suggest(event.target, true);
	}, true);

	document.addEventListener('change', function(event)
	{
		saveField(event.target);
	}, true);

	document.addEventListener('focusout', function(event)
	{
		if (pendingFields.has(event.target))
		{
			saveField(event.target);
		}
	}, true);

	window.addEventListener('pagehide', flushPendingFields, true);
	send('ready', {});
	suggest(document.activeElement, true);
})(window);
