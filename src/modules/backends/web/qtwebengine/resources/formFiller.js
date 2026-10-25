(function()
{
	const fields = %1;
	const inputs = document.querySelectorAll('input[name]');
	const hasPasswordField = fields.some(function(field)
	{
		return field.type === 'password' && Array.from(inputs).some(function(input)
		{
			return input.name === field.name && input.type === 'password';
		});
	});

	if (!hasPasswordField)
	{
		return;
	}

	for (let i = 0; i < fields.length; ++i)
	{
		for (let j = 0; j < inputs.length; ++j)
		{
			if (inputs[j].name === fields[i].name && (fields[i].type !== 'password' || inputs[j].type === 'password'))
			{
				const input = inputs[j];
				const valueSetter = Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype, 'value').set;
				valueSetter.call(input, fields[i].value);
				input.dispatchEvent(new Event('input', { bubbles: true }));
				input.dispatchEvent(new Event('change', { bubbles: true }));

				break;
			}
		}
	}
})();
