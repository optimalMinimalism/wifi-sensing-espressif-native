(function () {
    var NARROW_MAX = 1100;
    var tables = document.querySelectorAll('.wy-table-responsive');
    if (!tables.length) {
        return;
    }

    var isZh = (document.documentElement.lang || '').toLowerCase().indexOf('zh') === 0;
    var labels = {
        preview: isZh ? '预览全表' : 'Preview table',
        close: isZh ? '关闭' : 'Close',
        hint: isZh ? '缩放预览 · 点击背景关闭' : 'Scaled preview · click outside to close'
    };

    var modal = document.createElement('div');
    modal.className = 'table-preview-modal';
    modal.setAttribute('hidden', '');
    modal.innerHTML =
        '<div class="table-preview-modal__backdrop"></div>' +
        '<div class="table-preview-modal__dialog" role="dialog" aria-modal="true">' +
        '  <div class="table-preview-modal__header">' +
        '    <span class="table-preview-modal__hint"></span>' +
        '    <button type="button" class="table-preview-modal__close"></button>' +
        '  </div>' +
        '  <div class="table-preview-modal__viewport">' +
        '    <div class="table-preview-modal__scale-wrap"></div>' +
        '  </div>' +
        '</div>';
    document.body.appendChild(modal);

    var hintEl = modal.querySelector('.table-preview-modal__hint');
    var closeBtn = modal.querySelector('.table-preview-modal__close');
    var backdrop = modal.querySelector('.table-preview-modal__backdrop');
    var scaleWrap = modal.querySelector('.table-preview-modal__scale-wrap');
    var viewport = modal.querySelector('.table-preview-modal__viewport');

    hintEl.textContent = labels.hint;
    closeBtn.textContent = labels.close;
    closeBtn.setAttribute('aria-label', labels.close);

    function isNarrow() {
        return window.innerWidth <= NARROW_MAX;
    }

    function updateButtons() {
        var narrow = isNarrow();
        tables.forEach(function (wrapper) {
            var btn = wrapper.querySelector('.table-preview-btn');
            if (!btn) {
                return;
            }
            btn.hidden = !narrow;
        });
    }

    function fitScaledTable(table) {
        table.style.width = 'max-content';
        table.style.tableLayout = 'auto';
        table.style.display = 'table';

        var maxW = viewport.clientWidth - 16;
        var maxH = viewport.clientHeight - 16;
        var width = table.offsetWidth;
        var height = table.offsetHeight;
        var scale = Math.min(maxW / width, maxH / height, 1);

        scaleWrap.style.width = width + 'px';
        scaleWrap.style.height = height + 'px';
        scaleWrap.style.transform = 'scale(' + scale + ')';
    }

    function openPreview(sourceWrapper) {
        var sourceTable = sourceWrapper.querySelector('table');
        if (!sourceTable) {
            return;
        }

        scaleWrap.innerHTML = '';
        var table = sourceTable.cloneNode(true);
        table.classList.add('table-preview-modal__table');
        scaleWrap.appendChild(table);
        modal.removeAttribute('hidden');
        document.body.classList.add('table-preview-open');

        requestAnimationFrame(function () {
            fitScaledTable(table);
        });
    }

    function closePreview() {
        modal.setAttribute('hidden', '');
        document.body.classList.remove('table-preview-open');
        scaleWrap.innerHTML = '';
    }

    tables.forEach(function (wrapper) {
        var btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'table-preview-btn';
        btn.textContent = labels.preview;
        btn.addEventListener('click', function () {
            openPreview(wrapper);
        });
        wrapper.insertBefore(btn, wrapper.firstChild);
    });

    closeBtn.addEventListener('click', closePreview);
    backdrop.addEventListener('click', closePreview);
    document.addEventListener('keydown', function (event) {
        if (event.key === 'Escape' && !modal.hasAttribute('hidden')) {
            closePreview();
        }
    });
    window.addEventListener('resize', function () {
        updateButtons();
        if (!modal.hasAttribute('hidden')) {
            var table = scaleWrap.querySelector('table');
            if (table) {
                fitScaledTable(table);
            }
        }
    });

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', updateButtons);
    } else {
        updateButtons();
    }
})();
