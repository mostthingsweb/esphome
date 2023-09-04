let style = document.createElement("style");

style.textContent = `
input[type="range"] {
    width: auto !important;
}

button[class="rnd"] {
    height: 4rem;
    width: 4rem;
}
`

document.getElementsByTagName("esp-app")[0].shadowRoot.querySelector("esp-entity-table").shadowRoot.appendChild(style);