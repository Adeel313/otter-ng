/*
 * Otter Browser: Web browser controlled by the user, not vice-versa.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
(function (hideImages)
{
	const wasHidden = window.__otterHideImages;
	window.__otterHideImages = hideImages;
	const styleId = 'otter-image-visibility-style';
	const apply = function ()
	{
		let style = document.getElementById(styleId);

		if (!window.__otterHideImages)
		{
			if (style)
			{
				style.remove();
			}

			return;
		}

		if (!style)
		{
			style = document.createElement('style');
			style.id = styleId;
			style.textContent = 'img:not(.otter-image-reloaded), input[type="image"], svg image { visibility: hidden !important; }'
				+ '*, *::before, *::after { background-image: none !important; border-image-source: none !important; list-style-image: none !important; }';
			(document.head || document.documentElement).appendChild(style);
		}

		if (!wasHidden)
		{
			document.querySelectorAll('img.otter-image-reloaded').forEach(image => image.classList.remove('otter-image-reloaded'));
		}
	};

	if (document.documentElement)
	{
		apply();
	}
	else
	{
		document.addEventListener('DOMContentLoaded', apply, {once: true});
	}
})(%1);
