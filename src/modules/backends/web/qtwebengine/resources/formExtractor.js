(function(window)
{
	function attachFormListener(form)
	{
		if (form.__otterPasswordListenerAttached)
		{
			return;
		}

		form.__otterPasswordListenerAttached = true;
		form.addEventListener('submit', function()
		{
			const fields = [];
			const inputs = Array.from(form.querySelectorAll('input:not([disabled])[name]'));
			const password = inputs.find(function(input)
			{
				return input.type === 'password' && input.value !== '';
			});

			if (!password)
			{
				return;
			}

			const usernameCandidates = inputs.filter(function(input)
			{
				return input !== password && input.value !== '' && (input.type === 'email' || input.type === 'tel' || input.type === 'text') && (input.compareDocumentPosition(password) & Node.DOCUMENT_POSITION_FOLLOWING) !== 0;
			});
			const username = usernameCandidates.find(function(input)
			{
				return input.autocomplete.split(/\s+/).includes('username');
			}) || usernameCandidates.find(function(input)
			{
				return input.type === 'email';
			}) || usernameCandidates[usernameCandidates.length - 1];

			if (username)
			{
				fields.push({ name: username.name, value: username.value, type: 'text' });
			}

			fields.push({ name: password.name, value: password.value, type: 'password' });

			if (fields.length)
			{
				const payload = btoa(unescape(encodeURIComponent(JSON.stringify({ url: window.location.href, fields: fields }))));
				console.info('otter-save-password:%1:' + payload);
			}
		});
	}

	function attachForms(node)
	{
		if (node.nodeType !== Node.ELEMENT_NODE)
		{
			return;
		}

		if (node.matches('form'))
		{
			attachFormListener(node);
		}

		node.querySelectorAll('form').forEach(attachFormListener);
	}

	document.querySelectorAll('form').forEach(attachFormListener);

	new MutationObserver(function(mutations)
	{
		mutations.forEach(function(mutation)
		{
			mutation.addedNodes.forEach(attachForms);
		});
	}).observe(document.documentElement, { childList: true, subtree: true });
})(window);
