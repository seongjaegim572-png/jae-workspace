(() => {
  const menu = document.querySelector('.menu-toggle');
  const nav = document.querySelector('.nav-links');
  if (menu && nav) {
    const setMenu = open => {
      nav.classList.toggle('open', open);
      menu.setAttribute('aria-expanded', String(open));
      menu.setAttribute('aria-label', window.portfolioI18n?.menuLabel(open) || window.PORTFOLIO_I18N?.source[open ? 'ui.navigation.close' : 'ui.navigation.open'] || menu.getAttribute('aria-label'));
      menu.textContent = open ? '×' : '☰';
    };
    menu.addEventListener('click', () => {
      setMenu(!nav.classList.contains('open'));
    });
    nav.querySelectorAll('a').forEach(link => link.addEventListener('click', () => setMenu(false)));
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && nav.classList.contains('open')) {
        setMenu(false);
        menu.focus();
      }
    });
    document.addEventListener('click', event => {
      if (!nav.contains(event.target) && !menu.contains(event.target)) setMenu(false);
    });
  }
  const reveal = document.querySelectorAll('.reveal');
  if ('IntersectionObserver' in window) {
    const observer = new IntersectionObserver(entries => entries.forEach(entry => {
      if (entry.isIntersecting) {entry.target.classList.add('visible');observer.unobserve(entry.target)}
    }), { threshold: .08 });
    reveal.forEach(item => observer.observe(item));
  } else reveal.forEach(item => item.classList.add('visible'));

  const form = document.getElementById('sibyl-form');
  if (!form) return;
  let lastInputs;
  const renderPlan = inputs => {
    const t = window.portfolioI18n?.t || ((key,values={}) => (window.PORTFOLIO_I18N?.source[key] || key).replace(/\{([A-Za-z]\w*)\}/g,(match,name)=>values[name]===undefined?match:String(values[name])));
    const genre = `sibyl.plan.genres.${inputs.genre}`;
    const goal = `sibyl.plan.goals.${inputs.goal}`;
    document.getElementById('sibyl-result-title').textContent = t('sibyl.plan.title',{name:inputs.name,goal:t(`${goal}.intro`)});
    const bodies = [
      t('sibyl.plan.video',{platform:inputs.platform,angle:t(`${genre}.angle`)}),
      t('sibyl.plan.community',{community:t(`${genre}.community`)}),
      `${t(`${goal}.action`)} ${t(`${goal}.measure`)} ${t('sibyl.plan.record')}`
    ];
    const container = document.getElementById('sibyl-ideas');
    container.replaceChildren();
    bodies.forEach((body,index) => {
      const card = document.createElement('div');card.className = 'sample-idea';
      const strong = document.createElement('strong');strong.textContent = t(`sibyl.plan.steps.${index+1}`);
      const p = document.createElement('span');p.textContent = body;
      card.append(strong,p);container.append(card);
    });
  };
  form.addEventListener('submit', event => {
    event.preventDefault();
    if (!form.reportValidity()) return;
    lastInputs = {
      name:document.getElementById('artist').value.trim() || window.portfolioI18n?.t('sibyl.form.default_artist_name') || window.PORTFOLIO_I18N?.source['sibyl.form.default_artist_name'],
      genre:document.getElementById('genre').value,
      goal:document.getElementById('goal').value,
      platform:document.getElementById('platform').value
    };
    renderPlan(lastInputs);
    const result = document.getElementById('sibyl-result');result.hidden = false;
    result.scrollIntoView({behavior:window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth',block:'nearest'});
  });
  document.addEventListener('portfolio:languagechange', () => {if (lastInputs) renderPlan(lastInputs)});
})();
