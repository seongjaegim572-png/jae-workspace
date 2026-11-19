// www.namwonmiraeyeonhap.com 으로 들어오면 www 없는 주소로 영구 이동(301).
// 구글이 같은 사이트를 두 개로 보지 않게 하려는 것. 나머지는 그대로 정적 파일을 보여준다.
export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.hostname.startsWith("www.")) {
      url.hostname = url.hostname.slice(4);
      return Response.redirect(url.toString(), 301);
    }
    return env.ASSETS.fetch(request);
  },
};
