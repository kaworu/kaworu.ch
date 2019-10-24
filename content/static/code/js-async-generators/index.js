/*
 * A simple NodeJS example using async generators.
 */
'use strict';

const debug   = require('debug')('search');
const parse   = require('parse-link-header');
const request = require('request-promise-native');


// GitHub code search result generator.
async function *search(q) {
    const get = request.defaults({
        method: 'GET',
        json: true, // parse the result as JSON
        resolveWithFullResponse: true,
        headers: {
            // see https://developer.github.com/v3/#user-agent-required
            'User-Agent': 'js async generator demo',
        },
    });

    // setup the first URL to be requested. The followings requests will use
    // the rel="next" URL found in the response Link HTTP header, see
    // https://developer.github.com/v3/guides/traversing-with-pagination/
    let url = new URL('https://api.github.com/search/code');
    url.searchParams.set('q', q);

    // Main loop going through the paginated results.
    while (url) {
        debug(`requesting ${url}`);
        const rsp = await get({ url });
        if (rsp.statusCode !== 200 /* OK */) {
            throw new Error(`expected 200 OK but got ${rsp.statusCode}`);
        }

        debug(`yielding ${rsp.body.items.length} items`);
        yield* rsp.body.items;

        const link = parse(rsp.headers.link);
        if (link && link.next) {
            // Setup the next page URL for the next loop iteration.
            url = new URL(link.next.url);
        } else {
            // We've reached the last page, exit the loop.
            url = null;
        }
    }
}


// await may only be used from inside an async function.
async function main() {
    // You may change the search query, but beware of GitHub search rate limit
    // https://developer.github.com/v3/search/#rate-limit
    for await (const { score, name, repository } of search('OpenBSD+user:kAworu')) {
        console.log(`→ ${score.toFixed(2)}: ${name} (in ${repository.full_name})`);
    }
}


main();
