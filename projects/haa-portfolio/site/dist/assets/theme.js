/* Keep theme controls independent of localization, fetch, and optional browser APIs. */
(function () {
  var root = document.documentElement;
  var toggle = document.getElementById('theme-toggle');
  if (!toggle) return;
  var override = null;
  var media = null;
  try { override = localStorage.getItem('jae-portfolio-theme'); } catch (_) {}
  try { if (typeof window.matchMedia === 'function') media = window.matchMedia('(prefers-color-scheme: dark)'); } catch (_) {}
  function apply(theme) {
    toggle.checked = theme === 'dark';
    root.setAttribute('data-theme', theme);
    var meta = document.querySelector('meta[name="theme-color"]');
    if (meta) meta.setAttribute('content', toggle.checked ? '#050d19' : '#ffffff');
    try { document.dispatchEvent(new CustomEvent('portfolio:themechange')); } catch (_) {}
  }
  toggle.addEventListener('change', function () {
    override = toggle.checked ? 'dark' : 'light';
    try { localStorage.setItem('jae-portfolio-theme', override); } catch (_) {}
    apply(override);
  });
  apply(override === 'light' || override === 'dark' ? override : media && media.matches ? 'dark' : 'light');
  function followSystem() {
    if (override !== 'light' && override !== 'dark') apply(media && media.matches ? 'dark' : 'light');
  }
  try {
    if (media && typeof media.addEventListener === 'function') media.addEventListener('change', followSystem);
    else if (media && typeof media.addListener === 'function') media.addListener(followSystem);
  } catch (_) { /* Manual control remains available when system monitoring is restricted. */ }
})();
