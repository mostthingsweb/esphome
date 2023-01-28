let style = document.createElement("style");

style.textContent = `input[type="range"] {
    width: auto !important;
}`

document.getElementsByTagName("esp-app")[0].shadowRoot.querySelector("esp-entity-table").shadowRoot.appendChild(style);