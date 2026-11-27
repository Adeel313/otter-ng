// Persistent rules also cover elements inserted after initial parsing.
(function (exceptions, selectors)
{
	// Replacing filters must replace the old stylesheet and its observer too.
	if (window.otterCosmeticFiltersObserver) window.otterCosmeticFiltersObserver.disconnect();
	const previous = document.getElementById('otter-cosmetic-filters');
	if (previous) previous.remove();
	const excluded = new Set(exceptions);
	const exception = exceptions.length ? ':not(:is(' + exceptions.join(',') + '))' : '';
	const collapse = 'display: none !important; height: 0 !important; min-height: 0 !important; max-height: 0 !important; margin: 0 !important; padding: 0 !important; border-width: 0 !important; overflow: hidden !important;';
	const install = () =>
	{
		if (!document.documentElement) return false;
		let style = document.getElementById('otter-cosmetic-filters');
		if (style && style.sheet) return true;
		style = document.createElement('style');
		style.id = 'otter-cosmetic-filters';
		document.documentElement.appendChild(style);
		// CNN reserves header height independently of the hidden ad element.
		// Apply only when the active filters hide its header advertisement.
		if ((location.hostname === 'cnn.com' || location.hostname.endsWith('.cnn.com')) &&
			selectors.includes('.ad-slot-header__wrapper') && !exceptions.includes('.ad-slot-header__wrapper'))
		{
			style.sheet.insertRule('.ad-slot-header__wrapper { display: none !important; height: 0 !important; min-height: 0 !important; }', style.sheet.cssRules.length);
			style.sheet.insertRule('.header__wrapper-outer, .header__wrapper-inner { height: auto !important; min-height: 0 !important; padding-top: 0 !important; }', style.sheet.cssRules.length);
		}
		for (const selector of selectors)
		{
			if (excluded.has(selector)) continue;
			try
			{
				// :not() preserves element exceptions without overriding site styles.
				style.sheet.insertRule('html :is(' + selector + ')' + exception + ' { ' + collapse + ' }', style.sheet.cssRules.length);
			}
			catch (error) { /* Unsupported extended selectors are ignored individually. */ }
		}
		return true;
	};
	install();
	// Watch only direct root/head changes, not page content. Some sites replace
	// the initial stylesheet during hydration; restore it without scanning the DOM.
	const observer = new MutationObserver(() =>
	{
		if (document.documentElement) observer.observe(document.documentElement, {childList: true});
		if (!document.getElementById('otter-cosmetic-filters')) install();
	});
	window.otterCosmeticFiltersObserver = observer;
	observer.observe(document, {childList: true});
	const watchRoot = () =>
	{
		install();
		if (document.documentElement) observer.observe(document.documentElement, {childList: true});
	};
	if (document.documentElement) watchRoot();
	else document.addEventListener('readystatechange', watchRoot, {once: true});
})([%1], [%2]);
