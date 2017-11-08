/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons;

import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.loader.IconLoader;
import org.mozilla.gecko.icons.preparation.Preparer;
import org.mozilla.gecko.icons.processing.Processor;
import org.robolectric.RuntimeEnvironment;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

@RunWith(TestRunner.class)
public class TestIconTask {
    @Test
    public void testGeneratorIsInvokedIfAllLoadersFail() {
        final List<IconLoader> loaders = Arrays.asList(
                createFailingLoader(),
                createFailingLoader(),
                createFailingLoader());

        final Bitmap bitmap = mock(Bitmap.class);
        final IconLoader generator = createSuccessfulLoader(bitmap);

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                loaders,
                Collections.<Processor>emptyList(),
                generator);

        final IconResponse response = task.call();

        // Verify all loaders have been tried
        for (IconLoader loader : loaders) {
            verify(loader).load(request);
        }

        // Verify generator was called
        verify(generator).load(request);

        // Verify response contains generated bitmap
        Assert.assertEquals(bitmap, response.getBitmap());
    }

    @Test
    public void testGeneratorIsNotCalledIfOneLoaderWasSuccessful() {
        final List<IconLoader> loaders = Collections.singletonList(
                createSuccessfulLoader(mock(Bitmap.class)));

        final IconLoader generator = createSuccessfulLoader(mock(Bitmap.class));

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                loaders,
                Collections.<Processor>emptyList(),
                generator);

        final IconResponse response = task.call();

        // Verify all loaders have been tried
        for (IconLoader loader : loaders) {
            verify(loader).load(request);
        }

        // Verify generator was NOT called
        verify(generator, never()).load(request);

        Assert.assertNotNull(response);
    }

    @Test
    public void testNoLoaderIsInvokedForRequestWithoutUrls() {
        final List<IconLoader> loaders = Collections.singletonList(
                createSuccessfulLoader(mock(Bitmap.class)));

        final Bitmap bitmap = mock(Bitmap.class);
        final IconLoader generator = createSuccessfulLoader(bitmap);

        final IconRequest request = createIconRequestWithoutUrls();

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                loaders,
                Collections.<Processor>emptyList(),
                generator);

        final IconResponse response = task.call();

        // Verify NO loaders have been called
        for (IconLoader loader : loaders) {
            verify(loader, never()).load(request);
        }

        // Verify generator was called
        verify(generator).load(request);

        // Verify response contains generated bitmap
        Assert.assertEquals(bitmap, response.getBitmap());
    }

    @Test
    public void testAllPreparersAreCalledBeforeLoading() {
        final List<Preparer> preparers = Arrays.asList(
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class));

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                preparers,
                createListWithSuccessfulLoader(),
                Collections.<Processor>emptyList(),
                createGenerator());

        task.call();

        // Verify all preparers have been called
        for (Preparer preparer : preparers) {
            verify(preparer).prepare(request);
        }
    }

    @Test
    public void testSubsequentLoadersAreNotCalledAfterSuccessfulLoad() {
        final Bitmap bitmap = mock(Bitmap.class);

        final List<IconLoader> loaders = Arrays.asList(
                createFailingLoader(),
                createFailingLoader(),
                createSuccessfulLoader(bitmap),
                createSuccessfulLoader(mock(Bitmap.class)),
                createFailingLoader(),
                createSuccessfulLoader(mock(Bitmap.class)));

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                loaders,
                Collections.<Processor>emptyList(),
                createGenerator());

        final IconResponse response = task.call();

        // First loaders are called
        verify(loaders.get(0)).load(request);
        verify(loaders.get(1)).load(request);
        verify(loaders.get(2)).load(request);

        // Loaders after successful load are not called
        verify(loaders.get(3), never()).load(request);
        verify(loaders.get(4), never()).load(request);
        verify(loaders.get(5), never()).load(request);

        Assert.assertNotNull(response);
        Assert.assertEquals(bitmap, response.getBitmap());
    }

    @Test
    public void testNoProcessorIsCalledForUnsuccessfulLoads() {
        final IconRequest request = createIconRequest();

        final List<IconLoader> loaders = createListWithFailingLoaders();

        final List<Processor> processors = Arrays.asList(
            createProcessor(),
            createProcessor(),
            createProcessor());

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                loaders,
                processors,
                createFailingLoader());

        task.call();

        // Verify all loaders have been tried
        for (IconLoader loader : loaders) {
            verify(loader).load(request);
        }

        // Verify no processor was called
        for (Processor processor : processors) {
            verify(processor, never()).process(any(IconRequest.class), any(IconResponse.class));
        }
    }

    @Test
    public void testAllProcessorsAreCalledAfterSuccessfulLoad() {
        final IconRequest request = createIconRequest();

        final List<Processor> processors = Arrays.asList(
                createProcessor(),
                createProcessor(),
                createProcessor());

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                createListWithSuccessfulLoader(),
                processors,
                createGenerator());

        IconResponse response = task.call();

        Assert.assertNotNull(response);

        // Verify that all processors have been called
        for (Processor processor : processors) {
            verify(processor).process(request, response);
        }
    }

    @Test
    public void testCallbackIsExecutedForSuccessfulLoads() {
        final IconCallback callback = mock(IconCallback.class);

        final IconRequest request = createIconRequest();
        request.setCallback(callback);

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                createListWithSuccessfulLoader(),
                Collections.<Processor>emptyList(),
                createGenerator());

        final IconResponse response = task.call();

        verify(callback).onIconResponse(response);
    }

    @Test
    public void testCallbackIsNotExecutedIfLoadingFailed() {
        final IconCallback callback = mock(IconCallback.class);

        final IconRequest request = createIconRequest();
        request.setCallback(callback);

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                createListWithFailingLoaders(),
                Collections.<Processor>emptyList(),
                createFailingLoader());

        task.call();

        verify(callback, never()).onIconResponse(any(IconResponse.class));
    }

    @Test
    public void testCallbackIsExecutedWithGeneratorResult() {
        final IconCallback callback = mock(IconCallback.class);

        final IconRequest request = createIconRequest();
        request.setCallback(callback);

        final IconTask task = new IconTask(
                request,
                Collections.<Preparer>emptyList(),
                createListWithFailingLoaders(),
                Collections.<Processor>emptyList(),
                createGenerator());

        final IconResponse response = task.call();

        verify(callback).onIconResponse(response);
    }

    @Test
    public void testTaskCancellationWhileLoading() {
        // We simulate the cancellation by injecting a loader that interrupts the thread.
        final IconLoader cancellingLoader = spy(new IconLoader() {
            @Override
            public IconResponse load(IconRequest request) {
                Thread.currentThread().interrupt();
                return null;
            }
        });

        final List<Preparer> preparers = createListOfPreparers();
        final List<Processor> processors = createListOfProcessors();

        final List<IconLoader> loaders = Arrays.asList(
                createFailingLoader(),
                createFailingLoader(),
                cancellingLoader,
                createFailingLoader(),
                createSuccessfulLoader(mock(Bitmap.class)));

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                preparers,
                loaders,
                processors,
                createGenerator());

        final IconResponse response = task.call();
        Assert.assertNull(response);

        // Verify that all preparers are called
        for (Preparer preparer : preparers) {
            verify(preparer).prepare(request);
        }

        // Verify that first loaders are called
        verify(loaders.get(0)).load(request);
        verify(loaders.get(1)).load(request);

        // Verify that our loader that interrupts the thread is called
        verify(loaders.get(2)).load(request);

        // Verify that all other loaders are not called
        verify(loaders.get(3), never()).load(request);
        verify(loaders.get(4), never()).load(request);

        // Verify that no processors are called
        for (Processor processor : processors) {
            verify(processor, never()).process(eq(request), any(IconResponse.class));
        }
    }

    @Test
    public void testTaskCancellationWhileProcessing() {
        final Processor cancellingProcessor = spy(new Processor() {
            @Override
            public void process(IconRequest request, IconResponse response) {
                Thread.currentThread().interrupt();
            }
        });

        final List<Preparer> preparers = createListOfPreparers();

        final List<IconLoader> loaders = Arrays.asList(
                createFailingLoader(),
                createFailingLoader(),
                createSuccessfulLoader(mock(Bitmap.class)));

        final List<Processor> processors = Arrays.asList(
                createProcessor(),
                createProcessor(),
                cancellingProcessor,
                createProcessor(),
                createProcessor());

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                preparers,
                loaders,
                processors,
                createGenerator());

        final IconResponse response = task.call();
        Assert.assertNull(response);

        // Verify that all preparers are called
        for (Preparer preparer : preparers) {
            verify(preparer).prepare(request);
        }

        // Verify that all loaders are called
        for (IconLoader loader : loaders) {
            verify(loader).load(request);
        }

        // Verify that first processors are called
        verify(processors.get(0)).process(eq(request), any(IconResponse.class));
        verify(processors.get(1)).process(eq(request), any(IconResponse.class));

        // Verify that cancelling processor is called
        verify(processors.get(2)).process(eq(request), any(IconResponse.class));

        // Verify that subsequent processors are not called
        verify(processors.get(3), never()).process(eq(request), any(IconResponse.class));
        verify(processors.get(4), never()).process(eq(request), any(IconResponse.class));
    }

    @Test
    public void testTaskCancellationWhilePerparing() {
        final Preparer failingPreparer = spy(new Preparer() {
            @Override
            public void prepare(IconRequest request) {
                Thread.currentThread().interrupt();
            }
        });

        final List<Preparer> preparers = Arrays.asList(
                mock(Preparer.class),
                mock(Preparer.class),
                failingPreparer,
                mock(Preparer.class),
                mock(Preparer.class));

        final List<IconLoader> loaders = createListWithSuccessfulLoader();
        final List<Processor> processors = createListOfProcessors();

        final IconRequest request = createIconRequest();

        final IconTask task = new IconTask(
                request,
                preparers,
                loaders,
                processors,
                createGenerator());

        final IconResponse response = task.call();
        Assert.assertNull(response);

        // Verify that first preparers are called
        verify(preparers.get(0)).prepare(request);
        verify(preparers.get(1)).prepare(request);

        // Verify that cancelling preparer is called
        verify(preparers.get(2)).prepare(request);

        // Verify that subsequent preparers are not called
        verify(preparers.get(3), never()).prepare(request);
        verify(preparers.get(4), never()).prepare(request);

        // Verify that no loaders are called
        for (IconLoader loader : loaders) {
            verify(loader, never()).load(request);
        }

        // Verify that no processors are called
        for (Processor processor : processors) {
            verify(processor, never()).process(eq(request), any(IconResponse.class));
        }
    }

    @Test
    public void testNoLoadersOrProcessorsAreExecutedForPrepareOnlyTasks() {
        final List<Preparer> preparers = createListOfPreparers();
        final List<IconLoader> loaders = createListWithSuccessfulLoader();
        final List<Processor> processors = createListOfProcessors();
        final IconLoader generator = createGenerator();

        final IconRequest request = createIconRequest()
                .modify()
                .prepareOnly()
                .build();

        final IconTask task = new IconTask(
                request,
                preparers,
                loaders,
                processors,
                generator);

        IconResponse response = task.call();

        Assert.assertNull(response);

        // Verify that all preparers are called
        for (Preparer preparer : preparers) {
            verify(preparer).prepare(request);
        }

        // Verify that no loaders are called
        for (IconLoader loader : loaders) {
            verify(loader, never()).load(request);
        }

        // Verify that no processors are called
        for (Processor processor : processors) {
            verify(processor, never()).process(eq(request), any(IconResponse.class));
        }
    }

    public List<IconLoader> createListWithSuccessfulLoader() {
        return Arrays.asList(
                createFailingLoader(),
                createFailingLoader(),
                createSuccessfulLoader(mock(Bitmap.class)),
                createFailingLoader());
    }

    public List<IconLoader> createListWithFailingLoaders() {
        return Arrays.asList(
                createFailingLoader(),
                createFailingLoader(),
                createFailingLoader(),
                createFailingLoader(),
                createFailingLoader());
    }

    public List<Preparer> createListOfPreparers() {
        return Arrays.asList(
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class),
                mock(Preparer.class));
    }

    public IconLoader createFailingLoader() {
        final IconLoader loader = mock(IconLoader.class);
        doReturn(null).when(loader).load(any(IconRequest.class));
        return loader;
    }

    public IconLoader createSuccessfulLoader(Bitmap bitmap) {
        IconResponse response = IconResponse.create(bitmap);

        final IconLoader loader = mock(IconLoader.class);
        doReturn(response).when(loader).load(any(IconRequest.class));
        return loader;
    }

    public List<Processor> createListOfProcessors() {
        return Arrays.asList(
                mock(Processor.class),
                mock(Processor.class),
                mock(Processor.class),
                mock(Processor.class),
                mock(Processor.class));
    }

    public IconRequest createIconRequest() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon("http://www.mozilla.org/favicon.ico"))
                .build();
    }

    public IconRequest createIconRequestWithoutUrls() {
        return Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .build();
    }

    public IconLoader createGenerator() {
        return createSuccessfulLoader(mock(Bitmap.class));
    }

    public Processor createProcessor() {
        return mock(Processor.class);
    }
}
