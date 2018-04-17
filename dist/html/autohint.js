$(document).ready(function() {
	var flag = false;
	var dict_content = $("#dict_content");
	var qword = $("#qwt");
	var formobj = $("#qwFORM");
	qword.autocomplete({
		//autoFocus:true,
		source:function(req, res) {
			$.ajax({
				url:"neigh",
				type:"GET",
				dataType:"text",
				data:{"w":req.term,"off":"0","len":"10"},
				success:function(data) {
					res(data.split("\n"));
				},
				error:function() {
					res(["ERROR"]);
				}
			});
		},
		close:function(e,ui) {
			if (flag)
				formobj.submit();
			flag = false;
		},
		select:function(e,ui) {
			flag = true;
		}
	});
	function loadcontent(cnt) {
		dict_content.html(cnt);
		$("a").click(function(e) {
			if (this.href.indexOf("w=") >= 0) {
				e.preventDefault();
				var targetword = this.href.replace(/^.*w=([^&]+).*$/, "$1");
				qword.val(decodeURI(targetword));
				formobj.submit();
			}
		});
		window.scrollTo(0,0);
	}
	formobj.on("submit", function(e) {
		e.preventDefault();
		$.ajax({
			url:".",
			type:"GET",
			dataType:"html",
			data:{"w":qword.val(),"co":"c"},
			success:function(data) {
				loadcontent(data);
			},
			error:function(d,txt) {
				loadcontent(txt);
			}
		});
		return false;
	});
});
